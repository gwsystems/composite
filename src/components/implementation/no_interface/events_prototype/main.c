#include <pthread.h>

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>

#include <sl.h>
#include <sl_lock.h>
#include <sl_thd.h>

#include <res_spec.h>

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


/* This is a stub that rust calls to "assign_thread_data". It's fine for it to
 * do nothing for now
 */

/* struct plenty_of_data {
 *	char data[8 * 1024];
 * };
 * struct plenty_of_data backing_thread_data[SL_MAX_NUM_THDS];
 * void *                thread_data[SL_MAX_NUM_THDS];
*/

void
assign_thread_data(struct sl_thd *thread)
{
	/* struct cos_compinfo *ci     = cos_compinfo_get(cos_defcompinfo_curr_get());
	 * thdcap_t             thdcap = thread->thdcap;
	 * thdid_t              thdid  = thread->thdid;
	 *
	 * thread_data[thdid] = &backing_thread_data[thdid].data;
	 * printc("thdcap is %d\n", (int)thdcap);
	 * printc("Thread data is %p\n", thread_data[thdid]);
	 * printc("Thread data address is %p\n", &thread_data[thdid]);
	 * cos_thd_mod(ci, thdcap, &thread_data[thdid]);
	 */
}

extern void rust_init();

void
cos_init()
{
	printc("Entering rust!\n");
	rust_init();
	assert(0);
}
