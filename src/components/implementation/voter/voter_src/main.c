#include <cos_component.h>
#include <cobj_format.h>

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <res_spec.h>
#include <voter.h>

#include <sl.h>
#include <sl_lock.h>
#include <sl_thd.h>

#include <locale.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/******* CLEAN ME TEMP BOOTER STUBS ********/

// struct cobj_header *hs[MAX_NUM_SPDS + 1];

// void
// voter_boot_find_cobjs()
// {
// 	struct cobj_header *h = (struct cobj_header *)cos_comp_info.cos_poly[0];
// 	assert(cos_comp_info);
// 	int n = (int)cos_comp_info.cos_poly[1];
// 	printc("num objs %d\n",n);

// 	int     i;
// 	vaddr_t start, end;

// 	start = (vaddr_t)h;
// 	hs[0] = h;
// 	printc("header start %p\n",start);
// 	for (i = 1; i < n; i++) {
// 		int j = 0, size = 0, tot = 0;

// 		size = h->size;
// 		for (j = 0; j < (int)h->nsect; j++) {
// 			tot += cobj_sect_size(h, j);
// 		}
// 		printc("cobj %s:%d found at %p:%x, -> %x\n", h->name, h->id, hs[i - 1], size,
// 		       cobj_sect_get(hs[i - 1], 0)->vaddr);

// 		end   = start + round_up_to_cacheline(size);
// 		hs[i] = h = (struct cobj_header *)end;
// 		start     = end;
// 	}

// 	hs[n] = NULL;
// 	printc("cobj %s:%d found at %p -> %x\n", hs[n - 1]->name, hs[n - 1]->id, hs[n - 1],
// 	       cobj_sect_get(hs[n - 1], 0)->vaddr);
// }

/******* ^^CLEAN ME TEMP BOOTER STUBS ********/


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

void
print_hack(int n) {
	printc("rust hit %d \n",n);
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
	printc("%s - %d\n",__FILE__,__LINE__);
	struct cos_compinfo *ci     = cos_compinfo_get(cos_defcompinfo_curr_get());
	printc("%s - %d\n",__FILE__,__LINE__);
	thdcap_t             thdcap = sl_thd_thdcap(thread);
	printc("%s - %d\n",__FILE__,__LINE__);
	thdid_t              thdid  = thread->thdid;
	printc("%s - %d\n",__FILE__,__LINE__);

	/* HACK: We setup some thread specific data to make musl stuff work with sl threads */
	backing_thread_data[thdid].tid = thdid;
	printc("%s - %d\n",__FILE__,__LINE__);
	backing_thread_data[thdid].robust_list.head = &backing_thread_data[thdid].robust_list.head;
	backing_thread_data[thdid].tsd = calloc(PTHREAD_KEYS_MAX, sizeof(void*));

	thread_data[thdid] = &backing_thread_data[thdid];
	printc("%s - %d\n",__FILE__,__LINE__);


	cos_thd_mod(ci, thdcap, &thread_data[thdid]);
	printc("%s - %d\n",__FILE__,__LINE__);
}

extern void rust_init();
extern void test_call_rs();
extern void interface_handeler(int);

int rust_initialized = 0;

void
test_call(void)
{
	while (!rust_initialized);
	printc("Settting up thd data\n");
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_aep_info    *aep = cos_sched_aep_get(dci);
	assert(aep);

	struct sl_thd * thd = sl_thd_curr();
	assert(thd);
	thd->aepinfo = aep;
	assign_thread_data(thd);
	printc("Making rust call\n");
	//interface_handeler(0);
	test_call_rs();
	printc("Finished rust call\n");
	return;
}

void
cos_init()
{
	//printc("Voter: Booting Replicas\n");
	//voter_boot_find_cobjs();

	printc("Entering rust\n");
	rust_initialized = 1; // this is obvi still vulnerable to preemtpion - temp work around
	rust_init();
}

