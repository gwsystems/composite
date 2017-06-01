/* For printing: */

#include <cos_config.h>

#include <stdio.h>
#include <string.h>
#include <ck_pr.h>

#include <cos_component.h>

int
prints(char *s)
{
	int len = strlen(s);
	cos_print(s, len);
	return len;
}

int __attribute__((format(printf,1,2)))
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
#include <cobj_format.h>
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
static unsigned long n_kern_memsets = 0;

vaddr_t get_kmem_cap(void) {
	vaddr_t ret;

	ret = kmem_heap;
	kmem_heap += PAGE_SIZE;

	if (unlikely(kmem_heap >= BOOT_MEM_KM_BASE + PAGE_SIZE * RETYPE_MEM_NPAGES * n_kern_memsets))
		return 0;

	return ret;
}

static vaddr_t pmem_heap = BOOT_MEM_KM_BASE;

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

capid_t alloc_capid(cap_t cap);

/* This gets a virtual page on heap and the capability to a physical
 * page. Then maps it in. */
vaddr_t get_pmem(void)
{
	int ret;
	vaddr_t heap_vaddr, pmem_cap;

	pmem_cap = get_pmem_cap();
	if (!pmem_cap) return 0;

	heap_vaddr = (vaddr_t)cos_get_heap_ptr();
	if ((heap_vaddr & ~PGD_MASK) == 0) {
		capid_t pte_cap;
		vaddr_t ptemem_cap;

		pte_cap    = alloc_capid(CAP_PGTBL);
		ptemem_cap = get_kmem_cap();
		assert(pte_cap && ptemem_cap);

		/* PTE */
		if ((ret = call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_PGTBLACTIVATE,
				       pte_cap, BOOT_CAPTBL_SELF_PT, ptemem_cap, 1))) {
			printc("LLBOOT err ret %d: could not activate pte_cap %ld\n", ret, pte_cap);
			return 0;
		}

		/* Construct pgtbl */
		if ((ret = call_cap_op(BOOT_CAPTBL_SELF_PT, CAPTBL_OP_CONS, pte_cap, heap_vaddr, 0, 0))) {
			printc("LLBOOT err ret %d: could not cons captbl pte_cap\n", ret);
			return 0;
		}
	}

	if (heap_vaddr - SERVICE_START > SERVICE_SIZE * (1+BOOTER_NREGIONS)) {
		printc("Booter out of VAS: heap %p\n", (void *)heap_vaddr);
		return 0;
	}
	ret = call_cap_op(BOOT_CAPTBL_SELF_PT, CAPTBL_OP_MEMACTIVATE,
			  pmem_cap, BOOT_CAPTBL_SELF_PT, heap_vaddr, 0);
	if (ret) return 0;

	cos_set_heap_ptr((void *)(heap_vaddr + PAGE_SIZE));
	/* printc("heap_vaddr %x, npages %d\n", heap_vaddr, (heap_vaddr - BOOT_MEM_VM_BASE)/PAGE_SIZE); */

	return heap_vaddr;
}

CACHE_ALIGNED static u32_t liv_id_heap = BOOT_LIVENESS_ID_BASE;

u32_t get_liv_id(void) {
	u32_t ret;

	/* atomic fetch and add */
	ret = ck_pr_faa_uint(&liv_id_heap, 1);

	return ret;
}

static vaddr_t
__local_mman_get_page(spdid_t spd, vaddr_t addr, int flags)
{
	return 0;
	/* int frame_id; */

	/* if (flags & MAPPING_KMEM) { */
	/* 	frame_id = kern_frame_frontier++; */
	/* } else { */
	/* 	frame_id = frame_frontier++; */
	/* } */

	/* if (cos_mmap_cntl(COS_MMAP_GRANT, flags, cos_spd_id(), addr, frame_id)) BUG(); */

	/* return addr; */
}

static vaddr_t
__local_mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr, int flags)
{
	return 0;
	/* int frame_id; */

	/* if (flags & MAPPING_KMEM) { */
	/* 	frame_id = kern_frame_frontier - 1; */
	/* } else { */
	/* 	frame_id = frame_frontier - 1; */
	/* } */

	/* if (cos_mmap_cntl(COS_MMAP_GRANT, flags, d_spd, d_addr, frame_id)) BUG(); */

	/* return d_addr; */
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

/* We have 5 pages for the captbl of llboot: 1/2 page for the top
 * level, 4+1/2 pages for the second level. */
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
	if (unlikely(capid_64b_free < capid_32b_free)) {
		printc("LLBOOT: not enough CAPTBL IDs.\n");
		return 0;
	}
	assert(ret);
	assert(capid_64b_free >= capid_32b_free);

	return ret;
}

struct comp_cap_info {
	/* By default 5 pages for the captbl of each comp: half page
	 * 1st level, and 4.5 pages 2nd level */
	capid_t captbl_cap[BOOT_CAPTBL_NPAGES];
	capid_t pgtbl_cap;
	capid_t comp_cap;
	capid_t cap_frontier;
	vaddr_t addr_start;
	vaddr_t vaddr_mapped_in_booter; /* the address mapped into booter. */
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

#define CAPTBL_LEAFSZ  16
#define CAPTBL_INIT_SZ (PAGE_SIZE/2/16)

capid_t per_core_thd_cap[NUM_CPU_COS];
vaddr_t per_core_thd_mem[NUM_CPU_COS];

#define BOOT_INIT_SCHED_COMP 2
#define BOOT_INIT_MM_COMP 3

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
			llboot->init_thd, sched_comp->captbl_cap[0], thd_schedinit, 0)) BUG();
	if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_CPY,
			llboot->alpha, sched_comp->captbl_cap[0], thd_alpha, 0))        BUG();
	ck_spinlock_ticket_unlock(&init_lock);

	printc("Core %ld, Low-level booter created threads:\n"
	       "\tCap %d: alpha\n\tCap %d: init\n",
	       cos_cpuid(), llboot->alpha, llboot->init_thd);
	assert(llboot->init_thd >= 0);
}

static inline void
boot_comp_mm_init(void)
{
	vaddr_t memcap, kmem_cap, mm_memcap;
	capid_t pte_cap;
	unsigned long int n_frames, kmem;
	int ret, i;
	struct comp_cap_info *mm_comp = &comp_cap_info[BOOT_INIT_MM_COMP];

	if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_CPY,
			mm_comp->pgtbl_cap, mm_comp->captbl_cap[0], MM_CAPTBL_OWN_PGTBL, 0)) BUG();

	if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_CPY,
			mm_comp->captbl_cap[0], mm_comp->captbl_cap[0], MM_CAPTBL_OWN_CAPTBL, 0)) BUG();

	for (i = 1; i <= MAX_NUM_COMPS; i++) {
		if (i == cos_spd_id()) continue;

		/* Grant pgtbl cap to mem_mgr */
		if (comp_cap_info[i].pgtbl_cap) {
			if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_CPY,
					comp_cap_info[i].pgtbl_cap, mm_comp->captbl_cap[0],
					MM_CAPTBL_OWN_PGTBL + i * captbl_idsize(CAP_COMP), 0)) BUG();
		}
	}

	n_frames = 0;
	/* Grant the rest of the memory to the mem_mgr component. */
	while ((memcap = get_pmem_cap())) {
		mm_memcap = BOOT_MEM_KM_BASE + n_frames*PAGE_SIZE;
		if ((n_frames % (PAGE_SIZE/sizeof(void*))) == 0) {
			/* Need to expand PTE. */
			pte_cap  = alloc_capid(CAP_PGTBL);
			kmem_cap = get_kmem_cap();

			assert(kmem_cap);

			/* PTE */
			if ((ret = call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_PGTBLACTIVATE,
					       pte_cap, BOOT_CAPTBL_SELF_PT, kmem_cap, 1))) {
				printc("LLBOOT err ret %d: could not activate pte_cap %ld\n", ret, pte_cap);
				break;
			}

			/* Construct pgtbl */
			if ((ret = call_cap_op(mm_comp->pgtbl_cap, CAPTBL_OP_CONS, pte_cap, mm_memcap, 0, 0))) {
				printc("LLBOOT err ret %d: could not cons captbl pte_cap\n", ret);
				break;
			}
		}
		n_frames++;

		/* and grant memory cap by moving */
		if ((ret = call_cap_op(BOOT_CAPTBL_SELF_PT, CAPTBL_OP_MEMMOVE,
				       memcap, mm_comp->pgtbl_cap, mm_memcap, 0)))
			break;
	}

	/* Kernel memory next. */
	kmem = 0;
	/* Grant the rest of the memory to the mem_mgr component. */
	while ((memcap = get_kmem_cap())) {
		mm_memcap = BOOT_MEM_KM_BASE + kmem*PAGE_SIZE;
		if ((kmem % (PAGE_SIZE/sizeof(void*))) == 0) {
			/* Need to expand PTE. */
			pte_cap  = alloc_capid(CAP_PGTBL);
			kmem_cap = get_kmem_cap();
			/* PTE */
			if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_PGTBLACTIVATE,
					pte_cap, BOOT_CAPTBL_SELF_PT, kmem_cap, 1))    BUG();
			/* Construct pgtbl */
			if (call_cap_op(mm_comp->pgtbl_cap, CAPTBL_OP_CONS, pte_cap, mm_memcap, 0, 0)) BUG();
		}

		kmem++;
		/* and grant memory cap by moving */
		if ((ret = call_cap_op(BOOT_CAPTBL_SELF_PT, CAPTBL_OP_MEMMOVE,
				       memcap, mm_comp->pgtbl_cap, mm_memcap, 0)))
			break;
	}

	printc("LLBooter: granted %lu frames and %lu kernel pages to mem_mgr\n", n_frames, kmem);
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
	boot_comp_mm_init();

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

static void
quiescence_wait(void)
{
	u64_t s,e;
	rdtscll(s);
	while (1) {
		rdtscll(e);
		if (QUIESCENCE_CHECK(e, s, KERN_QUIESCENCE_CYCLES)) break;
	}
}

static void
tlb_quiescence_wait(void)
{
	u64_t s,e;
	rdtscll(s);
	while (1) {
		rdtscll(e);
		if (QUIESCENCE_CHECK(e, s, TLB_QUIESCENCE_CYCLES)) break;
	}
}


void comp_deps_run_all(void)
{
	sync_all();

	printc("Core %ld: low-level booter switching to init thread (cap %d).\n",
	       cos_cpuid(), PERCPU_GET(llbooter)->init_thd);

	/* switch to the init thd in the scheduler. */
	if (cap_switch_thd(PERCPU_GET(llbooter)->init_thd)) BUG();

	/* ... and we are back. Ready to exit. */
	sync_all();
	printc("Core %ld: exiting system from low-level booter.\n", cos_cpuid());

	return;
}

void cos_init(void);

int
sched_init_linux(void)
{
	assert(cos_cpuid() < NUM_CPU_COS);
	if (cos_cpuid() == INIT_CORE) {
		if (!PERCPU_GET(llbooter)->init_thd) {
			cos_init();
			assert(PERCPU_GET(llbooter)->init_thd);
		} else {
			comp_deps_run_all();
		}
	} else {
		LOCK();
		boot_comp_thds_init();
		UNLOCK();
		comp_deps_run_all();
	}

	return 0;
}

int
sched_init_i386(void)
{
	assert(cos_cpuid() < NUM_CPU_COS);

	if (cos_cpuid() == INIT_CORE) {
		cos_init();
		assert(PERCPU_GET(llbooter)->init_thd);
		comp_deps_run_all();
	} else {
		LOCK();
		boot_comp_thds_init();
		UNLOCK();
		comp_deps_run_all();
	}

	return 0;
}

#include <cpu_ghz.h>
int
sched_init(void)
{
#ifdef COS_LINUX
#error "GAP: COS_LINUX should not be defined."
	return sched_init_linux();
#else
	return sched_init_i386();
#endif
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
