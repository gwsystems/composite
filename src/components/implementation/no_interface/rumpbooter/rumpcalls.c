#include <stdio.h>
#include <string.h>
#include <cos_component.h>
#include <cos_alloc.h>
#include <cos_kernel_api.h>
#include <cos_types.h>

#include "rumpcalls.h"
#include "rump_cos_alloc.h"
#include "cos_sched.h"
#include "cos_lock.h"

extern struct cos_rumpcalls crcalls;
int boot_thread = 1;

/* Thread id */
volatile capid_t cos_cur = 0;
extern signed int cos_isr;

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

	/* These should be removed, confirm that they are never used */
	crcalls.rump_memcalloc        		= cos_memcalloc;
	crcalls.rump_memalloc         		= cos_memalloc;


	crcalls.rump_cos_thdid        		= cos_thdid;
	crcalls.rump_memcpy           		= memcpy;
	crcalls.rump_memset			= cos_memset;
	crcalls.rump_cpu_sched_create 		= cos_cpu_sched_create;

	if(!crcalls.rump_cpu_sched_create) printc("SCHED: rump_cpu_sched_create is set to null");

	crcalls.rump_cpu_sched_switch_viathd    = cos_cpu_sched_switch;
	crcalls.rump_memfree			= cos_memfree;
	crcalls.rump_tls_init 			= cos_tls_init;
	crcalls.rump_va2pa			= cos_vatpa;
	crcalls.rump_pa2va			= cos_pa2va;
	crcalls.rump_resume                     = cos_resume;
	crcalls.rump_platform_exit		= cos_vm_exit;

	crcalls.rump_intr_enable		= intr_enable;
	crcalls.rump_intr_disable		= intr_disable;
	return;
}

/* irq */
void
cos_irqthd_handler(void *line)
{
	int which = (int)line;
	thdid_t tid;
	int rcving;
	cycles_t cycles;

	while(1) {
		cos_rcv(irq_arcvcap[which], &tid, &rcving, &cycles);

		intr_delay(irq_thdcap[which]);

		bmk_isr(which);

		/* 
		 * cos_isr is set to zero when intrrupts are reenabled
		 * If cos_isr is zero but we don't finish this function, intr_pending will
		 * recognize this and switch back for us.
		 */
	}
}

/* Memory */
extern unsigned long bmk_memsize;
void
rump_bmk_memsize_init(void)
{
	/* (1<<20) == 1 MG */
	bmk_memsize = COS_VIRT_MACH_MEM_SZ - ((1<<20)*2);
	printc("FIX ME: ");
	printc("bmk_memsize: %lu\n", bmk_memsize);
}

void
cos_memfree(void *cp)
{
	rump_cos_free(cp);
}

void *
cos_memcalloc(size_t n, size_t size)
{

	printc("cos_memcalloc was called\n");
	while(1);

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
	printc("cos_memalloc was called\n");
	while(1);

	void *rv;

	rv = rump_cos_malloc(nbytes);

	return rv;
}

/*---- Scheduling ----*/
extern struct cos_compinfo booter_info;
int boot_thd = BOOT_CAPTBL_SELF_INITTHD_BASE;

void
cos_tls_init(unsigned long tp, thdcap_t tc)
{
	cos_thd_mod(&booter_info, tc, tp);
}

void
cos_cpu_sched_create(struct bmk_thread *thread, struct bmk_tcb *tcb,
		void (*f)(void *), void *arg,
		void *stack_base, unsigned long stack_size)
{

	thdcap_t newthd_cap;
	int ret;
	static int all_threads_offset = 0;
	//struct thd_creation_protocol  info;
	//struct thd_creation_protocol *thd_meta = &info;
	struct thd_creation_protocol *thd_meta = &all_rkthreads[all_threads_offset];

	all_threads_offset++;

	if(all_threads_offset >= 200) {
		printc("Did I tell the RK that it could have more than 200 threads?! NO. SO NO MORE THREADS FOR YOU!\n");
		assert(0);
	}

	if(boot_thread || !strcmp(get_name(thread), "isrthr")) {

		if(!strcmp(get_name(thread), "main")) boot_thread = 0;

		thd_meta->retcap = BOOT_CAPTBL_SELF_INITTHD_BASE;

	} else thd_meta->retcap = get_cos_thdcap(bmk_current);

	thd_meta->f = f;
	thd_meta->arg = arg;

	newthd_cap = cos_thd_alloc(&booter_info, booter_info.comp_cap, rump_thd_fn, thd_meta);
	set_cos_thdcap(thread, newthd_cap);

	/* To access the thd_id */
	printc("About to cos_switch at cos_cpu_sched_create on: %d\n", newthd_cap);
	ret = cos_switch(newthd_cap, 0, 0, 0, BOOT_CAPTBL_SELF_INITRCV_BASE);
	if(ret) printc("cos_cpu_sched_create cos_switch FAILED: %s\n", strerror(ret));
}


/* Called from RK init thread. The one in while(1) */

void
cos_resume(void)
{	
	thdid_t tid = 0;
	int rcving = 0;
	cycles_t cycles = 0;
	int pending = 0;
	int ret;

	while(1) {
		/* cos_rcv returns the number of pending messages */
		pending = cos_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &tid, &rcving, &cycles);

		/* 
		 * Handle all possible interrupts when intrupts are disabled
		 * Do we need to check every time we return here?
                 */
		if( (!intr_getdisabled(cos_isr)) ) intr_pending(pending, tid, rcving);

		ret = cos_switch(cos_cur, 0, 0, 0, BOOT_CAPTBL_SELF_INITRCV_BASE);
		if(ret) printc("cos_resume, cos_switch FAILED: %d\n", ret);
	}
}

void
cos_cpu_sched_switch(struct bmk_thread *unsused, struct bmk_thread *next)
{
	int ret;
	int temp = get_cos_thdcap(next);
	cos_cur = temp;


	ret = cos_switch(temp, 0, 0, 0, BOOT_CAPTBL_SELF_INITRCV_BASE);
	if(ret) printc("cos_cpu_sched_switch, cos_switch FAILED: %s\n", strerror(ret));
}

/* --------- Timer ----------- */

/* Convert to us from ns to us to give us more accuracy when dividing */
long long cycles_us = (long long)(CPU_GHZ * 1000);

/* Return monotonic time since RK initiation in nanoseconds */
long long
cos_cpu_clock_now(void)
{
	uint64_t tsc_now = 0;
	long long curtime = 0;

	rdtscll(tsc_now);

	/*
	 * We divide as we have cycles and cycles per nano,
	 * with unit analysis we need to divide to cancle cycles to just have ns
	 * The last thread in the timeq has < wakeup time.
	 */

	curtime = (long long)(tsc_now / cycles_us); /* cycles to us */
	curtime = (long long)(curtime * 1000); /* us to ns */

	return curtime;
}

void *
cos_vatpa(void * vaddr)
{ return cos_va2pa(&booter_info, vaddr); }

void *
cos_pa2va(void * pa, unsigned long len) 
{ return (void *)cos_hw_map(&booter_info, BOOT_CAPTBL_SELF_INITHW_BASE, (paddr_t)pa, (unsigned int)len); }

void
cos_vm_exit(void)
{ while (1) cos_thd_switch(VM_CAPTBL_SELF_EXITTHD_BASE); }
