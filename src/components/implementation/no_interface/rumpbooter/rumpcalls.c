#include <stdio.h>
#include <string.h>
#include <cos_component.h>
#include <cos_alloc.h>

#include <cos_kernel_api.h>

#include "rumpcalls.h"
#include "rump_cos_alloc.h"
#include "cos_sched.h"

//#define FP_CHECK(void(*a)()) ( (a == null) ? printc("SCHED: ERROR, function pointer is null.>>>>>>>>>>>\n");: printc("nothing");)

extern struct cos_rumpcalls crcalls;
int boot_thread = 1;

/* Mapping the functions from rumpkernel to composite */

void
cos2rump_setup(void)
{
	rump_bmk_memsize_init();

	crcalls.rump_cpu_clock_now 		= cos_cpu_clock_now;
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
	crcalls.rump_tls_init 			= cos_tls_init;
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
	rump_cos_free(cp);
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
	void *rv;

	rv = rump_cos_malloc(nbytes);

	return rv;
}

/*---- Scheduling ----*/
//struct bmk_thread *bmk_threads[MAX_NUM_THREADS];
extern struct cos_compinfo booter_info;
int boot_thd = BOOT_CAPTBL_SELF_INITTHD_BASE;

void
cos_tls_init(unsigned long tp, thdcap_t tc)
{
	cos_thd_mod(&booter_info, tc, tp);
}


/* RG: For debugging / lazy purposes, we use this name global variable
 * to keep track of the name we are giving the thread we are about to create
 * It is located within sched.c on the RK side
 */

void
cos_cpu_sched_create(struct bmk_thread *thread, struct bmk_tcb *tcb,
		void (*f)(void *), void *arg,
		void *stack_base, unsigned long stack_size)
{

	//printc("thdname: %s\n", get_name(thread));

	thdcap_t newthd_cap;
	int ret;
	struct thd_creation_protocol  info;
	struct thd_creation_protocol *thd_meta = &info;

	//  bmk_current is not set for the booting thread, use the booter_info thdcap_t
	//  The isrthr needs to be created on the cos thread.
	if(boot_thread || !strcmp(get_name(thread), "isrthr")) {

		if(!strcmp(get_name(thread), "main")) boot_thread = 0;

		thd_meta->retcap = BOOT_CAPTBL_SELF_INITTHD_BASE;

	} else thd_meta->retcap = get_cos_thdcap(bmk_current);

	thd_meta->f = f;
	thd_meta->arg = arg;

	newthd_cap = cos_thd_alloc(&booter_info, booter_info.comp_cap, rump_thd_fn, thd_meta);

	// To access the thd_id
	ret = cos_thd_switch(newthd_cap);
	if(ret) printc("cos_thd_switch FAILED\n");

	set_cos_thdcap(thread, newthd_cap);

	/*
	 * printc("\n------\nNew thread %d @ %x\n------\n\n",
	 *		(int)newthd_cap,
	 *		cos_introspect(&booter_info, newthd_cap, 0));
	*/
}

struct bmk_thread *glob_prev;
struct bmk_thread *glob_next;

void
cos_cpu_sched_switch(struct bmk_thread *prev, struct bmk_thread *next)
{
	glob_prev = prev;
	glob_next = next;

	//printc("\ncos_cpu_sched_switch\n");

	struct thd_creation_protocol info;
	struct thd_creation_protocol *thd_meta = &info;
	int ret;


	thd_meta->retcap = get_cos_thdcap(next);

	/* For Debugging
	 * printc("\n------\nSwitching thread to %d @ %x\n------\n\n",
	 *		(int)(thd_meta->retcap),
	 *		cos_introspect(&booter_info, thd_meta->retcap, 0));
	 */

	//printc("prev: %s\n", get_name(prev));
	//printc("next: %s\n", get_name(next));
	//printc("retcap: %d\n\n", thd_meta->retcap);

	ret = cos_thd_switch(thd_meta->retcap);
	if(ret)
		printc("thread switch failed\n");
}

/* --------- Timer ----------- */

/* Return monotonic time since RK initiation in nanoseconds */
long long
cos_cpu_clock_now(void)
{
	uint64_t tsc_now;

	rdtscll(tsc_now);
	//printc("tsc_now: %llu\n", tsc_now);

	return (long long)tsc_now;
}
