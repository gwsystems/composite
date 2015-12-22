#include <stdio.h>
#include <string.h>
#include <cos_component.h>
#include <cos_alloc.h>

#include <cos_kernel_api.h>

#include "rumpcalls.h"
#include "rump_cos_alloc.h"
#include "cos_sched.h"

//#define FP_CHECK(void(*a)()) ( (a == null) ? printc("SCHED: ERROR, function pointer is null.>>>>>>>>>>>\n");: printc("nothing");)

void cos_bmk_platform_cpu_sched_settls(
		unsigned long btcb_sp,
		unsigned long btcb_ip,
		unsigned long btcb_tp,
		unsigned long btcb_tpsize);

extern struct cos_rumpcalls crcalls;

/* Mapping the functions from rumpkernel to composite */

void
cos2rump_setup(void)
{
	rump_bmk_memsize_init();

	crcalls.rump_cos_print 	      		= cos_print;
	crcalls.rump_vsnprintf        		= vsnprintf;
	crcalls.rump_strcmp           		= strcmp;
	crcalls.rump_strncpy          		= strncpy;
	crcalls.rump_memcalloc        		= cos_memcalloc;
	crcalls.rump_memalloc         		= cos_memalloc;
	//crcalls.rump_pgalloc          	= alloc_page;
	crcalls.rump_cos_thdid        		= cos_thdid;
	crcalls.rump_memcpy           		= memcpy;
	crcalls.rump_memset			= cos_memset;
	crcalls.rump_cpu_sched_create 		= cos_cpu_sched_create;
	if(!crcalls.rump_cpu_sched_create){
		printc("SCHED: rump_cpu_sched_create is set to null");
	}
	crcalls.rump_cpu_sched_switch_viathd    = cos_cpu_sched_switch;
	crcalls.rump_memfree			= cos_memfree;
	crcalls.rump_bmk_platform_cpu_sched_settls = cos_bmk_platform_cpu_sched_settls;
	return;
}

/* Memory */
extern unsigned long bmk_memsize;
void
rump_bmk_memsize_init(void)
{
	/* (1<<20) == 1 MG */
	bmk_memsize = COS_MEM_USER_PA_SZ - ((1<<20)*2);
}

void
cos_memfree(void *cp)
{
	printc("\n\ncos_memfree\n\n");
	rump_cos_free(cp);
	printc("\n\ncos_memfree has returned\n\n");
}

void *
cos_memcalloc(size_t n, size_t size)
{

	void *rv;
	size_t tot = n * size;

	if (size != 0 && tot / size != n)
		return NULL;

	rv = rump_cos_calloc(n, size);
	return rv;
}

void *
cos_memalloc(size_t nbytes, size_t align)
{

	/* align is not taken into account as of right now */

	void *rv;

	rv = rump_cos_malloc(nbytes);

	return rv;
}

/*---- Scheduling ----*/
struct bmk_thread *bmk_threads[MAX_NUM_THREADS];
extern struct cos_compinfo booter_info;

void
cos_cpu_sched_create(struct bmk_thread *thread, struct bmk_tcb *tcb,
		void (*f)(void *), void *arg,
		void *stack_base, unsigned long stack_size)
{
	printc("SCHED: COS: cos_cpu_sched_create\n");
	thdcap_t newthd_cap;
	int ret;
	struct thd_creation_protocol  info;
	struct thd_creation_protocol *thd_meta = &info;

	//  This will be null for creating the first thread, use the booter_info thdcap_t
	if(bmk_current == NULL)
		thd_meta->retcap = BOOT_CAPTBL_SELF_INITTHD_BASE;
	else
		thd_meta->retcap = get_cos_thdcap(bmk_current);

	thd_meta->f = f;
	thd_meta->arg = arg;

	newthd_cap = cos_thd_alloc(&booter_info, booter_info.comp_cap, rump_thd_fn, thd_meta);

	printc("SCHED: COS: cos_cpu_sched_create, post thd_alloc\n");
	// To access the thd_id
	ret = cos_thd_switch(newthd_cap);
	if(ret)
		printc("cos_thd_switch fail\n");
	bmk_threads[*(thd_meta->thdid)] = thread;

	set_cos_thdcap(thread, newthd_cap);
}

void
cos_cpu_sched_switch(struct bmk_thread *prev, struct bmk_thread *next){
	printc("SCHED: COS: cos_cpu_sched_switch\n");
	struct thd_creation_protocol  *info;
	int ret;

	info->retcap = get_cos_thdcap(next);

	ret = cos_thd_switch(info->retcap);
	if(ret)
		printc("thread switch failed\n");
}
