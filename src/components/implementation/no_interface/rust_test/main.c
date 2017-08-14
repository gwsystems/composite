#include <pthread.h>

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>

#include <sl.h>
#include <sl_lock.h>
#include <sl_thd.h>

#include <res_spec.h>

// These are macro values rust needs, so we duplicate them here
vaddr_t       boot_mem_km_base            = BOOT_MEM_KM_BASE;
unsigned long cos_mem_kern_pa_sz          = COS_MEM_KERN_PA_SZ;
pgtblcap_t    boot_captbl_self_untyped_pt = BOOT_CAPTBL_SELF_UNTYPED_PT;

// This are wrappers for static inline functions that rust needs
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


// This is super ugly and stupid, so we put in C instead of rust
struct plenty_of_data {
	char data[8 * 1024];
};
struct plenty_of_data backing_thread_data[SL_MAX_NUM_THDS];
void *                thread_data[SL_MAX_NUM_THDS];

void
assign_thread_data(struct sl_thd *thread)
{
	printc("Assigning thread data to %d\n", (int)thread->thdid);

	struct cos_compinfo *ci     = cos_compinfo_get(cos_defcompinfo_curr_get());
	thdcap_t             thdcap = thread->thdcap;
	thdid_t              thdid  = thread->thdid;

	thread_data[thdid] = &backing_thread_data[thdid].data;
	printc("thdcap is %d\n", (int)thdcap);
	printc("Thread data is %p\n", thread_data[thdid]);
	printc("Thread data address is %p\n", &thread_data[thdid]);


	cos_thd_mod(ci, thdcap, &thread_data[thdid]);
	// cos_thd_mod(ci, thdcap, &thread_data[thdid]);
}

void
print_gs()
{
	void *data;
	__asm__ __volatile__("movl %%gs:0,%0" : "=r"(data));
	printc("Thdid is %d\n", (int)sl_thdid());
	printc("gs is %p\n", data);
}

// extern void rust_init();

void *
test_fun(void *x)
{
	printc("I live!\n");
	while (1) {
	}
}

void
cos_init()
{
	printc("trying pthread create...\n");

	pthread_t thread;
	int       return_val = pthread_create(&thread, NULL, test_fun, NULL);
	if (return_val) {
		printc("pthread_create failed %d\n", return_val);
	}
	while (1) {
	}
	// printc("Entering rust!\n");
	// rust_init();
	// assert(0);
}
