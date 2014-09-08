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

void 
failure_notif_fail(spdid_t caller, spdid_t failed);

int 
fault_page_fault_handler(spdid_t spdid, void *fault_addr, int flags, void *ip)
{
	unsigned long r_ip; 	/* the ip to return to */
	int tid = cos_get_thd_id();
	struct llbooter_per_core *llboot = PERCPU_GET(llbooter);

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
		llboot->recover_spd = spdid;
		llboot->prev_thd = cos_get_thd_id();
		cos_switch_thread(llboot->recovery_thd, 0);
		/* after the recovery thread is done, it should switch back to us. */
		return 0;
	}
	/* 
	 * The thread was created in the failed component...just use
	 * it to restart the component!  This might even be the
	 * initial thread.
	 */
	cos_upcall(spdid, 0); 	/* FIXME: give back stack... */
	BUG();

	return 0;
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

vaddr_t get_kmem_cap(void) {
	vaddr_t ret;

	ret = kmem_heap;
	kmem_heap += PAGE_SIZE;

	return ret;
}

static vaddr_t pmem_heap = BOOT_MEM_PM_BASE;

vaddr_t get_pmem_cap(void) {
	vaddr_t ret;

	ret = pmem_heap;
	pmem_heap += PAGE_SIZE;

	return ret;
}

static u64_t liv_id_heap = BOOT_LIVENESS_ID_BASE;

u64_t get_liv_id(void) {
	u64_t ret;
	ret = liv_id_heap++;
	
	return ret;
}

static vaddr_t
__local_mman_get_page(spdid_t spd, vaddr_t addr, int flags)
{
	int frame_id;

	if (flags & MAPPING_KMEM) {
		frame_id = kern_frame_frontier++;
	} else {
		frame_id = frame_frontier++;
	}

	if (cos_mmap_cntl(COS_MMAP_GRANT, flags, cos_spd_id(), addr, frame_id)) BUG();

	return addr;
}

static vaddr_t
__local_mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr, int flags)
{
	int frame_id;

	if (flags & MAPPING_KMEM) {
		frame_id = kern_frame_frontier - 1;
	} else {
		frame_id = frame_frontier - 1;
	}

	if (cos_mmap_cntl(COS_MMAP_GRANT, flags, d_spd, d_addr, frame_id)) BUG();

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

static void
boot_deps_run_all(void)
{
	assert(PERCPU_GET(llbooter)->init_thd);
	cos_switch_thread(PERCPU_GET(llbooter)->init_thd, 0);
	return ;
}

/*********************************************/
/* Functions using new cap operations below. */
/*********************************************/

/* We have 2 pages for the captbl of llboot: 1/2 page for the top
 * level, 1+1/2 pages for the second level. */
#define CAP_ID_32B_FREE BOOT_CAPTBL_FREE;            // goes up
#define CAP_ID_64B_FREE ((PAGE_SIZE + PAGE_SIZE/2)/32 - CAP64B_IDSZ) // goes down

capid_t capid_32b_free = CAP_ID_32B_FREE;
capid_t capid_64b_free = CAP_ID_64B_FREE;

capid_t alloc_capid(cap_t cap)
{
	capid_t ret;
	
	if (captbl_idsize(cap) == CAP32B_IDSZ) {
		ret = capid_32b_free;
		capid_32b_free += CAP32B_IDSZ;
	} else if (captbl_idsize(cap) == CAP64B_IDSZ) {
		ret = capid_64b_free;
		capid_64b_free -= CAP64B_IDSZ;
	} else if (captbl_idsize(cap) == CAP16B_IDSZ) {
		/* uncommon case: only sret is 16B, and we usually use
		 * cap 0 for sret. */
		if (capid_32b_free % CAPMAX_ENTRY_SZ) {
			capid_32b_free = round_up_to_pow2(capid_32b_free, CAPMAX_ENTRY_SZ);
		}

		ret = capid_32b_free;
		capid_32b_free += CAPMAX_ENTRY_SZ;
	} else {
		ret = 0;
		BUG();
	}
	assert(ret);
	assert(capid_64b_free >= capid_32b_free);

	return ret;
}

struct comp_cap_info {
	capid_t captbl_cap;
	capid_t pgtbl_cap;
	capid_t comp_cap;
	capid_t cap_frontier;
	vaddr_t addr_start;
	vaddr_t upcall_entry;
};

struct comp_cap_info comp_cap_info[MAX_NUM_SPDS+1];

#include <ck_spinlock.h>
#include <ck_pr.h>
/* Needed to avoid cas failure. */
ck_spinlock_ticket_t init_lock = CK_SPINLOCK_TICKET_INITIALIZER;

int synced_nthd = 0;
void sync_all()
{
	int ret;

	ret = ck_pr_faa_int(&synced_nthd, 1);
	ret = (ret/NUM_CPU_COS + 1)*NUM_CPU_COS;
	while (ck_pr_load_int(&synced_nthd) < ret) ;
	
	return;
}

// PPOS test code below
static inline void
acap_test(void)
{
	int ret;
	/* asnd and arcv tests! */
	struct llbooter_per_core *llboot = PERCPU_GET(llbooter);
	struct comp_cap_info *ping = &comp_cap_info[2];
	struct comp_cap_info *pong = &comp_cap_info[3];

	//use the same cap id in ping and pong for simplicity. 
	capid_t async_sndthd_cap = SND_THD_CAP_BASE + captbl_idsize(CAP_THD)*cos_cpuid();
	capid_t async_rcvthd_cap = RCV_THD_CAP_BASE + captbl_idsize(CAP_THD)*cos_cpuid();
	capid_t async_test_cap   = ACAP_BASE + captbl_idsize(CAP_ARCV)*cos_cpuid();

	vaddr_t thd_mem;
	capid_t pong_thd_cap;

	/* lock to avoid cas failure. */
	ck_spinlock_ticket_lock(&init_lock);

	if (cos_cpuid() == INIT_CORE) {
		capid_t shmem = get_pmem_cap();
		int ret;
		//map this to ping and pong
		ret = call_cap_op(BOOT_CAPTBL_SELF_PT, CAPTBL_OP_CPY, 
				  shmem, ping->pgtbl_cap, ping->addr_start + 0x400000 - PAGE_SIZE, 0);
		if (ret) printc("map shmem to ping failed! ret %d\n", ret);
		ret = call_cap_op(BOOT_CAPTBL_SELF_PT, CAPTBL_OP_CPY, 
				  shmem, pong->pgtbl_cap, pong->addr_start + 0x400000 - PAGE_SIZE, 0);
		if (ret) printc("map shmem to pong failed! ret %d\n", ret);
	}

	thd_mem = get_kmem_cap();
	pong_thd_cap = alloc_capid(CAP_THD);

	// grant alpha thd to pong as well
	if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_CPY,
			llboot->alpha, pong->captbl_cap, SCHED_CAPTBL_ALPHATHD_BASE + captbl_idsize(CAP_THD)*cos_cpuid(), 0)) BUG();

	if (cos_cpuid() < (NUM_CPU_COS/2)) { // sending core
		// create rcv thd in ping. and copy it to ping's captbl.
		if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_THDACTIVATE, pong_thd_cap,
				BOOT_CAPTBL_SELF_PT, thd_mem, ping->comp_cap)) BUG();

		if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_CPY,
				pong_thd_cap, ping->captbl_cap, async_rcvthd_cap, 0)) BUG();

		// create asnd / arcv caps!
		if (call_cap_op(ping->captbl_cap, CAPTBL_OP_ARCVACTIVATE, async_test_cap, 
				pong_thd_cap, ping->comp_cap, 0)) BUG();

		if (call_cap_op(pong->captbl_cap, CAPTBL_OP_ASNDACTIVATE, async_test_cap, 
				ping->captbl_cap, async_test_cap, 0)) BUG();

	} else { // receiving core

		// create rcv thd in pong. and copy it to ping's captbl.
		if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_THDACTIVATE, pong_thd_cap,
				BOOT_CAPTBL_SELF_PT, thd_mem, pong->comp_cap)) BUG();

		if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_CPY,
				pong_thd_cap, ping->captbl_cap, async_rcvthd_cap, 0)) BUG();

		// create asnd / arcv caps!
		if (call_cap_op(pong->captbl_cap, CAPTBL_OP_ARCVACTIVATE, async_test_cap, 
				pong_thd_cap, pong->comp_cap, 0)) BUG();

		if (call_cap_op(ping->captbl_cap, CAPTBL_OP_ASNDACTIVATE, async_test_cap,
				pong->captbl_cap, async_test_cap, 0)) BUG();

	}

	ck_spinlock_ticket_unlock(&init_lock);

	/* printc("asnd/arcv %d caps created on core %ld\n", async_test_cap, cos_cpuid()); */

	return;
}

/* for ppos tests only */
int snd_rcv_order[NUM_CPU];
int run_ppos_test(void)
{
	int ret;
	//serialize the init order
	if (cos_cpuid() != INIT_CORE) 
		while (ck_pr_load_int(&snd_rcv_order[cos_cpuid()-1]) == 0) ;
	acap_test();
	ck_pr_store_int(&snd_rcv_order[cos_cpuid()], 1);

	//and sync
	sync_all(); 

//#define MEM_OP
#ifdef MEM_OP
//	if (cos_cpuid() != INIT_CORE && cos_cpuid() != INIT_CORE+SND_RCV_OFFSET) {
//	if (cos_cpuid() != INIT_CORE) {
	if (1){
		u64_t s,e;
		struct comp_cap_info *ping = &comp_cap_info[2];
		struct comp_cap_info *pong = &comp_cap_info[3];

		capid_t pmem = ping->addr_start + PAGE_SIZE;
		vaddr_t to_addr = ping->addr_start + 0x400000 - NUM_CPU*(PAGE_SIZE*16) + cos_cpuid()*PAGE_SIZE*16;
		int i, ret;
#define ITER (100*1000)//(10*1024*1024)
		rdtscll(s);
		for (i = 0; i < ITER; i++) {
			ret = call_cap_op(ping->pgtbl_cap, CAPTBL_OP_CPY,
					  pmem, ping->pgtbl_cap, to_addr, 0);
			/* if (ret) { */
			/* 	printc("ret %d ...on core %d\n", ret, cos_cpuid()); */
			/* 	continue; */
			/* } */
			assert(!ret);
			ret = call_cap_op(ping->pgtbl_cap, CAPTBL_OP_MAPPING_DECONS,
					  to_addr, 0, 0, 0);
			/* if (ret) { */
			/* 	printc("decons failed on core %d, ret %d\n", cos_cpuid(), ret); */
			/* 	continue; */
			/* } */
			assert(!ret);
		}
		rdtscll(e);
		printc("mem_op done on core %d, avg %llu\n", cos_cpuid(), (e-s)/ITER);
		return 1;
	}
#endif

//#define INTERFERE_CORE_ENABLE
#ifdef INTERFERE_CORE_ENABLE
	if (cos_cpuid() == NUM_CPU_COS-1) {
		/* perform interference! */
		struct llbooter_per_core *llboot = PERCPU_GET(llbooter);
		struct comp_cap_info *ping = &comp_cap_info[2];
		struct comp_cap_info *pong = &comp_cap_info[3];
		capid_t if_cap = IF_CAP_BASE + cos_cpuid()*captbl_idsize(CAP_THD);

		u64_t s,e;
		printc("core %d interfering @ capid %d...\n", cos_cpuid(), if_cap);
		rdtscll(s);
		while (1) {
			if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_CPY,
					llboot->init_thd, ping->captbl_cap, if_cap, 0)) {
				printc("failed...1\n");
				break;
			}
			if (call_cap_op(ping->captbl_cap, CAPTBL_OP_THDDEACTIVATE,
					if_cap, 0, 0, 0)) {
				printc("failed...2\n");
				break;
			}
			rdtscll(e);
			if ((e-s)/(2000*1000*1000) > RUNTIME) break;
		}
		printc("Core %ld: interference done. exiting system.\n", cos_cpuid());

		return 1;
	} 
#endif
	return 0;
}

//PPOS test code done.

capid_t per_core_thd_cap[NUM_CPU_COS];
vaddr_t per_core_thd_mem[NUM_CPU_COS];

#define BOOT_INIT_SCHED_COMP 2

static inline void
boot_comp_thds_init(void)
{
	struct comp_cap_info *sched_comp = &comp_cap_info[BOOT_INIT_SCHED_COMP];
	struct llbooter_per_core *llboot = PERCPU_GET(llbooter);
	capid_t thd_alpha, thd_schedinit;

	/* We reserve 2 caps for each core in the captbl of scheduler */
	thd_alpha     = SCHED_CAPTBL_ALPHATHD_BASE + cos_cpuid() * captbl_idsize(CAP_THD);
	thd_schedinit = SCHED_CAPTBL_INITTHD_BASE  + cos_cpuid() * captbl_idsize(CAP_THD);
	assert(thd_alpha && thd_schedinit);
	assert(thd_schedinit <= SCHED_CAPTBL_LAST);

	ck_spinlock_ticket_lock(&init_lock);
	llboot->alpha        = BOOT_CAPTBL_SELF_INITTHD_BASE + cos_cpuid() * captbl_idsize(CAP_THD);
	llboot->init_thd     = per_core_thd_cap[cos_cpuid()];
	if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_THDACTIVATE, llboot->init_thd, 
			BOOT_CAPTBL_SELF_PT, per_core_thd_mem[cos_cpuid()], sched_comp->comp_cap)) BUG();

	/* Scheduler should have access to the init thread and alpha
	 * thread. Grant caps by copying. */
	if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_CPY,
			llboot->init_thd, sched_comp->captbl_cap, thd_schedinit, 0)) BUG();
	if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_CPY,
			llboot->alpha, sched_comp->captbl_cap, thd_alpha, 0))        BUG();
	ck_spinlock_ticket_unlock(&init_lock);

	printc("Core %ld, Low-level booter created threads:\n"
	       "\tCap %d: alpha\n\tCap %d: init\n",
	       cos_cpuid(), llboot->alpha, llboot->init_thd);
	assert(llboot->init_thd >= 0);
}

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

static inline void
boot_comp_deps_init(void)
{
	int i;	

	alloc_per_core_thd();
	boot_comp_thds_init();

	/* How many memory managers are there? */
	for (i = 0 ; init_schedule[i] ; i++) nmmgrs += init_mem_access[i];
	assert(nmmgrs > 0);
}

static void
boot_deps_run(void)
{
	assert(cos_cpuid() == INIT_CORE);
	assert(PERCPU_GET(llbooter)->init_thd);

	return; /* We return to comp0 and release other cores first. */
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

#include <sched_hier.h>

void comp_deps_run_all(void)
{
	sync_all();

	/* PPOS test only. */
	if (run_ppos_test()) goto done;

	printc("Core %ld: low-level booter switching to init thread (cap %d).\n", 
	       cos_cpuid(), PERCPU_GET(llbooter)->init_thd);
	/* switch to the init thd in the scheduler. */
	if (cap_switch_thd(PERCPU_GET(llbooter)->init_thd)) BUG();
done:
	sync_all();
	printc("Core %ld: exiting system from low-level booter.\n", cos_cpuid());

	return;
}

void cos_init(void);

int sched_init(void)   
{
	assert(cos_cpuid() < NUM_CPU_COS);
	if (cos_cpuid() == INIT_CORE) {
		if (!PERCPU_GET(llbooter)->init_thd) cos_init();
		else comp_deps_run_all();
	} else {
		LOCK();
		boot_comp_thds_init();
		UNLOCK();
		comp_deps_run_all();
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

