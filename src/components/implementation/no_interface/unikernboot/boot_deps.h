/* For printing: */

#include <stdio.h>
#include <string.h>
#include <ck_pr.h>

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

#ifndef assert
/* On assert, immediately switch to the "exit" thread */
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); cos_switch_thread(PERCPU_GET(llbooter)->alpha, 0);} } while(0)
#endif

#ifdef BOOT_DEPS_H
#error "boot_deps.h should not be included more than once, or in anything other than boot."
#endif
#define BOOT_DEPS_H

#include <cos_component.h>
#include <res_spec.h>

#include <sched_hier.h>



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
} __attribute__((aligned(4096))); /* Avoid the copy-on-write issue for us. */

PERCPU(struct llbooter_per_core, llbooter);


enum { /* hard-coded initialization schedule */
	LLBOOT_SCHED = 2,
	LLBOOT_MM    = 3,
};

struct comp_boot_info {
	int symbols_initialized, initialized, memory_granted;
};
#define NCOMPS 6 	/* comp0, us, and the four other components */
static struct comp_boot_info comp_boot_nfo[NCOMPS];

static spdid_t init_schedule[]     = {LLBOOT_MM, LLBOOT_SCHED, 0};
static int     init_mem_access[]   = {1, 0, 0};
static int     nmmgrs              = 0;
static int     frame_frontier      = 0; /* which physical frames have we used? */
static int     kern_frame_frontier = 0; /* used physical frames for kernel */

typedef void (*crt_thd_fn_t)(void);

/* 
 * Abstraction layer around 1) synchronization, 2) scheduling and
 * thread creation, and 3) memory operations.  
 */

#include "../../sched/cos_sched_sync.h"
#include "../../sched/cos_sched_ds.h"
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
	struct llbooter_per_core *llboot = PERCPU_GET(llbooter);
	assert(llboot->alpha);
	/* 
	 * When the initial thread is done, then all we have to do is
	 * switch back to alpha who should reboot the system.
	 */
	if (tid == llboot->init_thd) {
		int offset = llboot->sched_offset;
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
				/* kernel mem */
				max_pfn = cos_pfn_cntl(COS_PFN_MAX_MEM_KERN, 0, 0, 0);
				proportion = (max_pfn - kern_frame_frontier)/nmmgrs;
				cos_pfn_cntl(COS_PFN_GRANT_KERN, s, kern_frame_frontier, proportion);

				comp_boot_nfo[s].memory_granted = 1;
			}
			llboot->sched_offset++;
			comp_boot_nfo[s].initialized = 1;
			
			/* printc("core %ld: booter init_thd upcalling into spdid %d.\n", cos_cpuid(), (unsigned int)s); */
			cos_upcall(s, 0); /* initialize the component! */
			BUG();
		}
		/* Done initializing; reboot!  If we are here, then
		 * all of the threads have terminated, thus there is
		 * no more execution left to do.  Technically, the
		 * other components should have called
		 * sched_exit... */
		printc("core %ld: booter init_thd switching back to alpha %d.\n", cos_cpuid(), llboot->alpha);

		while (1) cos_switch_thread(llboot->alpha, 0);
		BUG();
	}
	
	while (1) {
		int     pthd = llboot->prev_thd;
		spdid_t rspd = llboot->recover_spd;

		assert(tid == llboot->recovery_thd);
		if (rspd) {             /* need to recover a component */
			assert(pthd);
			llboot->recover_spd = 0;
			cos_upcall(rspd, 0); /* This will escape from the loop */
			assert(0);
		} else {		/* ...done reinitializing...resume */
			assert(pthd && pthd != tid);
			llboot->prev_thd = 0;   /* FIXME: atomic action required... */
			cos_switch_thread(pthd, 0);
		}
	}
}

/* memory operations... */

/* Not used. */
static vaddr_t init_hp = 0; 		/* initial heap pointer */
/* 
 * Virtual address to frame calculation...assume the first address
 * passed in is the start of the heap, and they only increase by a
 * page from there.
 */
static inline int
__vpage2frame(vaddr_t addr) { return (addr - init_hp) / PAGE_SIZE; }

/* 
 * Assumptions about the memory management functions: 
 * - we only get single-page-increasing virtual addresses to map into.
 * - we never deallocate memory.
 * - we allocate memory contiguously
 * Many of these assumptions are ensured by the following code.
 * cos_get_vas_page should allocate vas contiguously, and incrementing
 * by a page, and the free function is made empty.
 */

static vaddr_t kmem_heap = BOOT_MEM_KM_BASE;
static unsigned long n_kern_memsets = 0;

vaddr_t get_kmem_cap(void) {
	vaddr_t ret;

	ret = kmem_heap;
	kmem_heap += PAGE_SIZE;

	if (unlikely(kmem_heap >= BOOT_MEM_KM_BASE + PAGE_SIZE * RETYPE_MEM_NPAGES * n_kern_memsets))
		return 0;

	return ret;
}

static vaddr_t pmem_heap = BOOT_MEM_PM_BASE;

/* Only called by the init core. No lock / atomic op required. */
vaddr_t get_pmem_cap(void) {
	int ret;

	if (pmem_heap % RETYPE_MEM_SIZE == 0) {
		/* Retype this region as user memory before use. */
		ret = call_cap_op(BOOT_CAPTBL_SELF_PT, CAPTBL_OP_MEM_RETYPE2USER,
				  pmem_heap, 0, 0, 0);

		if (ret) return 0;
	}

	ret = pmem_heap;
	pmem_heap += PAGE_SIZE;

	return ret;
}

/* This gets a virtual page on heap and the capability to a physical
 * page. Then maps it in. */
vaddr_t get_pmem(void)
{
	int ret;
	vaddr_t heap_vaddr, pmem_cap;

	pmem_cap = get_pmem_cap();
	if (!pmem_cap) return 0;

	heap_vaddr = (vaddr_t)cos_get_heap_ptr();
	ret = call_cap_op(BOOT_CAPTBL_SELF_PT, CAPTBL_OP_MEMACTIVATE,
			  pmem_cap, BOOT_CAPTBL_SELF_PT, heap_vaddr, 0);
	if (ret) return 0;

	cos_set_heap_ptr((void *)(heap_vaddr + PAGE_SIZE));
	/* printc("heap_vaddr %x, npages %d\n", heap_vaddr, (heap_vaddr - BOOT_MEM_VM_BASE)/PAGE_SIZE); */

	return heap_vaddr;
}

static inline void boot_create_init_thds(void)
{
	struct llbooter_per_core *llboot = PERCPU_GET(llbooter);
	if (cos_sched_cntl(COS_SCHED_EVT_REGION, 0, (long)PERCPU_GET(cos_sched_notifications))) BUG();

	llboot->alpha        = cos_get_thd_id();
	llboot->recovery_thd = cos_create_thread(cos_spd_id(), 0, 0);
	assert(llboot->recovery_thd >= 0);
	llboot->init_thd     = cos_create_thread(cos_spd_id(), 0, 0);
	printc("Core %ld, Low-level booter created threads:\n\t"
	       "%d: alpha\n\t%d: recov\n\t%d: init\n",
	       cos_cpuid(), llboot->alpha, 
	       llboot->recovery_thd, llboot->init_thd);
	assert(llboot->init_thd >= 0);
}

/*********************************************/
/* Functions using new cap operations below. */
/*********************************************/

/* We have 2 pages for the captbl of llboot: 1/2 page for the top
 * level, 1+1/2 pages for the second level. */
#define CAP_ID_32B_FREE BOOT_CAPTBL_FREE;            // goes up
#define CAP_ID_64B_FREE ((PAGE_SIZE*BOOT_CAPTBL_NPAGES - PAGE_SIZE/2)/16 - CAP64B_IDSZ) // goes down

//capid_t capid_16b_free = CAP_ID_32B_FREE;
capid_t capid_32b_free = CAP_ID_32B_FREE;
capid_t capid_64b_free = CAP_ID_64B_FREE;

/* allocate a new capid in the booter. */
capid_t alloc_capid(cap_t cap)
{
	/* FIXME: an proper allocation method for 16, 32 and 64B caps. */
	capid_t ret;
	
	if (captbl_idsize(cap) == CAP32B_IDSZ) {
		ret = capid_32b_free;
		capid_32b_free += CAP32B_IDSZ;
	} else if (captbl_idsize(cap) == CAP64B_IDSZ 
		   || captbl_idsize(cap) == CAP16B_IDSZ) {
		/* 16B is the uncommon case. Only the sret is 16 bytes
		 * now. Use an entire cacheline for it as well. */

		ret = capid_64b_free;
		capid_64b_free -= CAP64B_IDSZ;
	} else {
		ret = 0;
		BUG();
	}
	assert(ret);
	assert(capid_64b_free >= capid_32b_free);

	return ret;
}

capid_t per_core_thd_cap[NUM_CPU_COS];
vaddr_t per_core_thd_mem[NUM_CPU_COS];

static inline void 
alloc_per_core_thd(void)
{
	int i;

	/* Only the init core does the resource allocation here. Thus
	 * no locking needed. */
	for (i = 0; i < NUM_CPU_COS; i++) {
		per_core_thd_cap[i] = alloc_capid(CAP_THD);
		per_core_thd_mem[i] = get_kmem_cap();
		assert(per_core_thd_cap[i] && per_core_thd_mem[i]);
	}
}

void 
cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	printc("core %ld: <<cos_upcall_fn thd %d (type %d, CREATE=%d, DESTROY=%d, FAULT=%d)>>\n",
	       cos_cpuid(), cos_get_thd_id(), t, COS_UPCALL_THD_CREATE, COS_UPCALL_DESTROY, COS_UPCALL_UNHANDLED_FAULT);

	switch (t) {
	case COS_UPCALL_THD_CREATE:
		llboot_ret_thd();
		break;
	case COS_UPCALL_DESTROY:
		llboot_thd_done();
		break;
	case COS_UPCALL_UNHANDLED_FAULT:
		printc("Core %ld: Fault detected by the llboot component in thread %d: "
		       "Major system error.\n", cos_cpuid(), cos_get_thd_id());
		break;
	default:
		printc("Core %ld: thread %d in llboot receives undefined upcall. Params: %d, %p, %p, %p\n", 
		       cos_cpuid(), cos_get_thd_id(), t, arg1, arg2, arg3);

		return;
	}

	return;
}

void cos_init(void);

#include <cpu_ghz.h>
int
sched_init(void)
{
	assert(cos_cpuid() < NUM_CPU_COS);

	if (cos_cpuid() == INIT_CORE) {
		cos_init();
		assert(PERCPU_GET(llbooter)->init_thd);
	}

	/* calling return cap */
	call_cap(0, 0, 0, 0, 0);

	return 0;
}

int  sched_isroot(void) { return 1; }
void
sched_exit(void)
{
	printc("LLBooter: Core %ld called sched_exit. Switching back to alpha.\n", cos_cpuid());
	while (1) cos_switch_thread(PERCPU_GET(llbooter)->alpha, 0);
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
