/* For printing: */

#include <stdio.h>
#include <string.h>

static int 
prints(char *s)
{
	int len = strlen(s);
	cos_print(s, len);
	return len;
}

static int __attribute__((format(printf,1,2))) 
printc(char *fmt, ...)
{
	char s[128];
	va_list arg_ptr;
	int ret, len = 128;

	va_start(arg_ptr, fmt);
	ret = vsnprintf(s, len, fmt, arg_ptr);
	va_end(arg_ptr);
	cos_print(s, ret);

	return ret;
}
/* On assert, immediately switch to the "exit" thread */
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); cos_switch_thread(per_core_llbooter[cos_cpuid()].alpha, 0);} } while(0)


#ifdef BOOT_DEPS_H
#error "boot_deps.h should not be included more than once, or in anything other than boot."
#endif
#define BOOT_DEPS_H

#include <cos_component.h>
//#include <print.h>
#include <res_spec.h>

struct llbooter_per_core {
	/* 
	 * alpha:        the initial thread context for the system
	 * init_thd:     the thread used for the initialization of the rest 
	 *               of the system
	 * recovery_thd: the thread to perform initialization/recovery
	 * prev_thd:     the thread requesting the initialization
	 * recover_spd:  the spd that will require rebooting
	 */
	int     alpha, init_thd, recovery_thd;
	int     sched_offset;      
	volatile int prev_thd, recover_spd;
} CACHE_ALIGNED;

static struct llbooter_per_core per_core_llbooter[MAX_NUM_CPU];

enum { /* hard-coded initialization schedule */
	LLBOOT_SCHED = 2,
	LLBOOT_MM    = 3,
};

struct comp_boot_info {
	int symbols_initialized, initialized, memory_granted;
};
#define NCOMPS 6 	/* comp0, us, and the four other components */
static struct comp_boot_info comp_boot_nfo[NCOMPS];

static spdid_t init_schedule[]   = {LLBOOT_MM, LLBOOT_SCHED, 0};
static int     init_mem_access[] = {1, 0, 0};
static int     nmmgrs            = 0;
static int     frame_frontier    = 0; /* which physical frames have we used? */

typedef void (*crt_thd_fn_t)(void);

/* 
 * Abstraction layer around 1) synchronization, 2) scheduling and
 * thread creation, and 3) memory operations.  
 */

#include "../../sched/cos_sched_sync.h"
/* synchronization... */
#define LOCK()   if (cos_sched_lock_take())    BUG();
#define UNLOCK() if (cos_sched_lock_release()) BUG();

/* scheduling/thread operations... */

/* We'll do all of our own initialization... */
static int
sched_create_thread_default(spdid_t spdid, u32_t v1, u32_t v2, u32_t v3)
{ return 0; }

static void
llboot_ret_thd(void) { return; }

/* 
 * When a created thread finishes, here we decide what to do with it.
 * If the system's initialization thread finishes, we know to reboot.
 * Otherwise, we know that recovery is complete, or should be done.
 */
static void
llboot_thd_done(void)
{
	int tid = cos_get_thd_id();

	assert(per_core_llbooter[cos_cpuid()].alpha);
	/* 
	 * When the initial thread is done, then all we have to do is
	 * switch back to alpha who should reboot the system.
	 */
	if (tid == per_core_llbooter[cos_cpuid()].init_thd) {
		int offset = per_core_llbooter[cos_cpuid()].sched_offset;
		spdid_t s = init_schedule[offset];

		/* Is it done, or do we need to initialize another component? */
		if (s) {
			/* If we have a memory manger, give it a
			 * proportional amount of memory WRT to the
			 * other memory managers. */
			if (init_mem_access[offset] && cos_cpuid() == INIT_CORE) {
				int max_pfn, proportion;

				max_pfn = cos_pfn_cntl(COS_PFN_MAX_MEM, 0, 0, 0);
				proportion = (max_pfn - frame_frontier)/nmmgrs;
				cos_pfn_cntl(COS_PFN_GRANT, s, frame_frontier, proportion);
				comp_boot_nfo[s].memory_granted = 1;
			}
			per_core_llbooter[cos_cpuid()].sched_offset++;
			comp_boot_nfo[s].initialized = 1;
			
			printc("core %ld: booter init_thd upcalling into spdid %d.\n", cos_cpuid(), (unsigned int)s);
			cos_upcall(s); /* initialize the component! */
			BUG();
		}
		/* Done initializing; reboot!  If we are here, then
		 * all of the threads have terminated, thus there is
		 * no more execution left to do.  Technically, the
		 * other components should have called
		 * sched_exit... */
		printc("core %ld: booter init_thd switching back to alpha %d.\n", cos_cpuid(), per_core_llbooter[cos_cpuid()].alpha);

		while (1) cos_switch_thread(per_core_llbooter[cos_cpuid()].alpha, 0);
		BUG();
	}
	
	while (1) {
		int     pthd = per_core_llbooter[cos_cpuid()].prev_thd;
		spdid_t rspd = per_core_llbooter[cos_cpuid()].recover_spd;
				
		assert(tid == per_core_llbooter[cos_cpuid()].recovery_thd);
		if (rspd) {             /* need to recover a component */
			assert(pthd);
			per_core_llbooter[cos_cpuid()].recover_spd = 0;
			cos_upcall(rspd); /* This will escape from the loop */
			assert(0);
		} else {		/* ...done reinitializing...resume */
			assert(pthd && pthd != tid);
			per_core_llbooter[cos_cpuid()].prev_thd = 0;   /* FIXME: atomic action required... */
			cos_switch_thread(pthd, 0);
		}
	}
}

void 
failure_notif_fail(spdid_t caller, spdid_t failed);

int 
fault_page_fault_handler(spdid_t spdid, void *fault_addr, int flags, void *ip)
{
	unsigned long r_ip; 	/* the ip to return to */
	int tid = cos_get_thd_id();

	failure_notif_fail(cos_spd_id(), spdid);
	/* no reason to save register contents... */
	if(!cos_thd_cntl(COS_THD_INV_FRAME_REM, tid, 1, 0)) {
		/* Manipulate the return address of the component that called
		 * the faulting component... */
		assert(r_ip = cos_thd_cntl(COS_THD_INVFRM_IP, tid, 1, 0));
		/* ...and set it to its value -8, which is the fault handler
		 * of the stub. */
		assert(!cos_thd_cntl(COS_THD_INVFRM_SET_IP, tid, 1, r_ip-8));

		/* switch to the recovery thread... */
		per_core_llbooter[cos_cpuid()].recover_spd = spdid;
		per_core_llbooter[cos_cpuid()].prev_thd = cos_get_thd_id();
		cos_switch_thread(per_core_llbooter[cos_cpuid()].recovery_thd, 0);
		/* after the recovery thread is done, it should switch back to us. */
		return 0;
	}
	/* 
	 * The thread was created in the failed component...just use
	 * it to restart the component!  This might even be the
	 * initial thread.
	 */
	cos_upcall(spdid); 	/* FIXME: give back stack... */
	BUG();

	return 0;
}

/* memory operations... */

static vaddr_t init_hp = 0; 		/* initial heap pointer */
/* 
 * Assumptions about the memory management functions: 
 * - we only get single-page-increasing virtual addresses to map into.
 * - we never deallocate memory.
 * - we allocate memory contiguously
 * Many of these assumptions are ensured by the following code.
 * cos_get_vas_page should allocate vas contiguously, and incrementing
 * by a page, and the free function is made empty.
 */

/* 
 * Virtual address to frame calculation...assume the first address
 * passed in is the start of the heap, and they only increase by a
 * page from there.
 */
static inline int
__vpage2frame(vaddr_t addr) { return (addr - init_hp) / PAGE_SIZE; }

static vaddr_t
__mman_get_page(spdid_t spd, vaddr_t addr, int flags)
{
	if (cos_mmap_cntl(COS_MMAP_GRANT, 0, cos_spd_id(), addr, frame_frontier++)) BUG();
	if (!init_hp) init_hp = addr;
	return addr;
}

static vaddr_t
__mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	int fp;

	assert(init_hp);
	fp = __vpage2frame(s_addr);
	assert(fp >= 0);
	if (cos_mmap_cntl(COS_MMAP_GRANT, 0, d_spd, d_addr, fp)) BUG();
	return d_addr;
}

static int boot_spd_set_symbs(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci);
static void
comp_info_record(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci) 
{ 
	if (!comp_boot_nfo[spdid].symbols_initialized) {
		comp_boot_nfo[spdid].symbols_initialized = 1;
		boot_spd_set_symbs(h, spdid, ci);
	}
}

static inline void boot_create_init_thds(void)
{
	if (cos_sched_cntl(COS_SCHED_EVT_REGION, 0, (long)&cos_sched_notifications[cos_cpuid()])) BUG();

	per_core_llbooter[cos_cpuid()].alpha        = cos_get_thd_id();
	per_core_llbooter[cos_cpuid()].recovery_thd = cos_create_thread((int)llboot_ret_thd, (int)0, 0);
	assert(per_core_llbooter[cos_cpuid()].recovery_thd >= 0);
	per_core_llbooter[cos_cpuid()].init_thd     = cos_create_thread((int)llboot_ret_thd, 0, 0);
	printc("Low-level booter created threads:\n\t"
	       "%d: alpha\n\t%d: recov\n\t%d: init\n",
	       per_core_llbooter[cos_cpuid()].alpha, per_core_llbooter[cos_cpuid()].recovery_thd, per_core_llbooter[cos_cpuid()].init_thd);
	assert(per_core_llbooter[cos_cpuid()].init_thd >= 0);
}

static void
boot_deps_init(void)
{
	int i;
	boot_create_init_thds();

	/* How many memory managers are there? */
	for (i = 0 ; init_schedule[i] ; i++) nmmgrs += init_mem_access[i];
	assert(nmmgrs > 0);
}

static void
boot_deps_run(void)
{
	assert(cos_cpuid() == INIT_CORE);
	assert(per_core_llbooter[cos_cpuid()].init_thd);
	return; /* We return to comp0 and release other cores first. */
	//cos_switch_thread(per_core_llbooter[cos_cpuid()].init_thd, 0);
}

static void
boot_deps_run_all(void)
{
	assert(per_core_llbooter[cos_cpuid()].init_thd);
	cos_switch_thread(per_core_llbooter[cos_cpuid()].init_thd, 0);
	return ;
}

void 
cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	/* printc("core %ld: <<cos_upcall_fn as %d (type %d, CREATE=%d, DESTROY=%d, FAULT=%d)>>\n",  */
	/*        cos_cpuid(), cos_get_thd_id(), t, COS_UPCALL_CREATE, COS_UPCALL_DESTROY, COS_UPCALL_UNHANDLED_FAULT); */
	switch (t) {
	case COS_UPCALL_CREATE:
		cos_argreg_init();
		llboot_ret_thd();
//		((crt_thd_fn_t)arg1)();
		break;
	case COS_UPCALL_DESTROY:
		llboot_thd_done();
		break;
	case COS_UPCALL_UNHANDLED_FAULT:
		printc("Fault detected by the llboot component in thread %d: "
		       "Major system error.\n", cos_get_thd_id());
	default:
		while (1) ;
		return;
	}

	return;
}

#include <sched_hier.h>

void cos_init(void);
int  sched_init(void)   
{ 
	if (cos_cpuid() == INIT_CORE) {
		/* The init core will call this function twice: first do
		 * the cos_init, then return to cos_loader and boot
		 * other cores, last call here again to run the init
		 * core. */
		if (!per_core_llbooter[cos_cpuid()].init_thd) cos_init();
		else boot_deps_run_all();
	} else {
		LOCK();
		boot_create_init_thds();
		UNLOCK();
		boot_deps_run_all();
		printc("core %ld, alpha: exiting system.\n", cos_cpuid());
	}
	return 0; 
}

int  sched_isroot(void) { return 1; }
void 
sched_exit(void)
{
	printc("LLBooter: Core %ld called sched_exit. Switching back to alpha.\n", cos_cpuid());
	while (1) cos_switch_thread(per_core_llbooter[cos_cpuid()].alpha, 0);	
}

int 
sched_child_get_evt(spdid_t spdid, struct sched_child_evt *e, int idle, unsigned long wake_diff) 
{ BUG(); return 0; }

int 
sched_child_cntl_thd(spdid_t spdid) 
{ 
	if (cos_sched_cntl(COS_SCHED_PROMOTE_CHLD, 0, spdid)) {BUG(); while(1);}
	/* printc("Grant thd %d to sched %d\n", cos_get_thd_id(), spdid); */
	if (cos_sched_cntl(COS_SCHED_GRANT_SCHED, cos_get_thd_id(), spdid)) BUG();
	return 0;
}

int 
sched_child_thd_crt(spdid_t spdid, spdid_t dest_spd) { BUG(); return 0; }
