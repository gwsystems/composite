#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <res_spec.h>

#include <sl.h>
#include <sl_lock.h>
#include <sl_thd.h>

#include <locale.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* These are macro values rust needs, so we duplicate them here */
vaddr_t       boot_mem_km_base            = BOOT_MEM_KM_BASE;
unsigned long cos_mem_kern_pa_sz          = COS_MEM_KERN_PA_SZ;
pgtblcap_t    boot_captbl_self_untyped_pt = BOOT_CAPTBL_SELF_UNTYPED_PT;

/* This are wrappers for static inline functions that rust needs */
sched_param_t
sched_param_pack_rs(sched_param_type_t type, unsigned int value)
{
	return sched_param_pack(type, value);
}

struct sl_thd *
sl_thd_curr_rs()
{
	return sl_thd_curr();
}

thdid_t
sl_thdid_rs()
{
	return sl_thdid();
}

struct sl_thd *
sl_thd_lkup_rs(thdid_t tid)
{
	return sl_thd_lkup(tid);
}

microsec_t
sl_cyc2usec_rs(cycles_t cyc)
{
	return sl_cyc2usec(cyc);
}

cycles_t
sl_usec2cyc_rs(microsec_t usec)
{
	return sl_usec2cyc(usec);
}

cycles_t
sl_now_rs()
{
	return sl_now();
}

microsec_t
sl_now_usec_rs()
{
	return sl_now_usec();
}

void
sl_lock_take_rs(struct sl_lock *lock)
{
	return sl_lock_take(lock);
}

void
sl_lock_release_rs(struct sl_lock *lock)
{
	return sl_lock_release(lock);
}

/* This is a bit of a hack, but we setup pthread data for sl threads */
#define _NSIG 65

struct pthread {
	struct pthread *self;
	void **dtv, *unused1, *unused2;
	uintptr_t sysinfo;
	uintptr_t canary, canary2;
	pid_t tid, pid;
	int tsd_used, errno_val;
	volatile int cancel, canceldisable, cancelasync;
	int detached;
	unsigned char *map_base;
	size_t map_size;
	void *stack;
	size_t stack_size;
	void *start_arg;
	void *(*start)(void *);
	void *result;
	struct __ptcb *cancelbuf;
	void **tsd;
	pthread_attr_t attr;
	volatile int dead;
	struct {
		volatile void *volatile head;
		long off;
		volatile void *volatile pending;
	} robust_list;
	int unblock_cancel;
	volatile int timer_id;
	locale_t locale;
	volatile int killlock[2];
	volatile int exitlock[2];
	volatile int startlock[2];
	unsigned long sigmask[_NSIG/8/sizeof(long)];
	char *dlerror_buf;
	int dlerror_flag;
	void *stdio_locks;
	uintptr_t canary_at_end;
	void **dtv_copy;
};

struct pthread backing_thread_data[SL_MAX_NUM_THDS];
void *         thread_data[SL_MAX_NUM_THDS];

void
assign_thread_data(struct sl_thd *thread)
{
	struct cos_compinfo *ci     = cos_compinfo_get(cos_defcompinfo_curr_get());
	thdcap_t             thdcap = sl_thd_thdcap(thread);
	thdid_t              thdid  = sl_thd_thdid(thread);

	/* HACK: We setup some thread specific data to make musl stuff work with sl threads */
	backing_thread_data[thdid].tid = thdid;
	backing_thread_data[thdid].robust_list.head = &backing_thread_data[thdid].robust_list.head;
	backing_thread_data[thdid].tsd = calloc(PTHREAD_KEYS_MAX, sizeof(void*));

	thread_data[thdid] = &backing_thread_data[thdid];
	cos_thd_mod(ci, thdcap, &thread_data[thdid]);
}

extern void rust_init();

void
cos_init()
{
	printc("Entering rust!\n");
	rust_init();
	assert(0);
}
