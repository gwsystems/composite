#ifdef BOOT_DEPS_H
#error "boot_deps.h should not be included more than once, or in anything other than boot."
#endif
#define BOOT_DEPS_H

#include <cos_component.h>
#include <print.h>
#include <res_spec.h>

/* 
 * Abstraction layer around 1) synchronization, 2) scheduling and
 * thread creation, and 3) memory operations.  
 */

#include "../../sched/cos_sched_sync.h"
/* synchronization... */
#define LOCK()   if (cos_sched_lock_take())    BUG();
#define UNLOCK() if (cos_sched_lock_release()) BUG();

/* scheduling/thread operations... */

#define INIT_SPD 2

/* 
 * alpha: the initial thread context for the system, 
 * init_thd: the thread used for the initialization of the rest of the
 * system, 
 * recovery_thd: the thread to perform initialization/recovery, 
 * prev_thd: the thread requesting the initialization, and
 * recover_spd: the spd that will require rebooting
 */
static int alpha, init_thd, recovery_thd;	
static volatile int prev_thd, recover_spd;

#define __sched_create_thread_default sched_create_thread_default

typedef void (*crt_thd_fn_t)(void);

static void
llboot_init_thd(void)
{
	cos_upcall(INIT_SPD);
	assert(0);		/* should not get past the upcall */
}

static int
sched_create_thread_default(spdid_t spdid, u32_t v1, u32_t v2, u32_t v3)
{
	if (spdid != INIT_SPD) return 1;
	assert(!init_thd);
	init_thd = cos_create_thread((int)llboot_init_thd, 0, 0);

	return 0;
}

static void 
llboot_recover_spd(void) { return; }

/* 
 * When a created thread finishes, here we decide what to do with it.
 * If the system's initialization thread finishes, we know to reboot.
 * Otherwise, we know that recovery is complete, or should be done.
 */
static void
llboot_thd_done(void)
{
	int tid = cos_get_thd_id();
	/* 
	 * When the initial thread is done, then all we have to do is
	 * switch back to alpha who should reboot the system.
	 */
	if (tid == init_thd) {
		assert(alpha);
		while (1) cos_switch_thread(alpha, 0);
	}
	
	while (1) {
		int     pthd = prev_thd;
		spdid_t rspd = recover_spd;
				
		assert(tid == recovery_thd);
		if (rspd) {             /* need to recover a component */
			assert(pthd);
			recover_spd = 0;
			cos_upcall(rspd); /* This will escape from the loop */
			assert(0);
		} else {		/* ...done reinitializing...resume */
			assert(pthd && pthd != tid);
			prev_thd = 0;   /* FIXME: atomic action required... */
			cos_switch_thread(pthd, 0);
		}
	}
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
	static int frame_frontier = 0;
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

static void
comp_info_record(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci) { return; }

static void
boot_deps_recover(spdid_t spdid)
{
	recover_spd = spdid;
	prev_thd = cos_get_thd_id();
	cos_switch_thread(recovery_thd, 0);
	/* after the recovery thread is done, it should switch back to us. */
}

static void
boot_deps_init(void)
{
	alpha        = cos_get_thd_id();
	recovery_thd = cos_create_thread((int)llboot_recover_spd, (int)0, 0);
	assert(recovery_thd >= 0);
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_CREATE:
		cos_argreg_init();
		((crt_thd_fn_t)arg1)();
		break;
	case COS_UPCALL_DESTROY:
		llboot_thd_done();
		break;
	case COS_UPCALL_UNHANDLED_FAULT:
	default:
		assert(0);
		return;
	}

	return;
}
