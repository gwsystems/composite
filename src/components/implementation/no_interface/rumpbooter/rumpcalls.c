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

//#define FP_CHECK(void(*a)()) ( (a == null) ? printc("SCHED: ERROR, function pointer is null.>>>>>>>>>>>\n");: printc("nothing");)

extern struct cos_rumpcalls crcalls;
int boot_thread = 1;
//void lock(int *i) {
//	while (!(*i)) ;
//	*i = 0;
//}
//
//void unlock(int *i) {
//	*i = 1;
//}

/* Thread id */
capid_t cos_cur = 0;
extern signed int cos_isr;

void rump2cos_rcv(void);

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
	if(!crcalls.rump_cpu_sched_create){
		printc("SCHED: rump_cpu_sched_create is set to null");
	}
	crcalls.rump_cpu_sched_switch_viathd    = cos_cpu_sched_switch;
	crcalls.rump_memfree			= cos_memfree;
	crcalls.rump_tls_init 			= cos_tls_init;
	crcalls.rump_va2pa			= cos_vatpa;
	crcalls.rump_pa2va			= cos_pa2va;
	crcalls.rump_resume                     = cos_resume;
	crcalls.rump_platform_exit		= cos_vm_exit;
	crcalls.rump_rcv 			= rump2cos_rcv;

	crcalls.rump_intr_enable		= intr_enable;
	crcalls.rump_intr_disable		= intr_disable;
	return;
}

/* send and recieve notifications */
void
rump2cos_rcv(void)
{
	
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
	set_cos_thdcap(thread, newthd_cap);
	// To access the thd_id
	ret = cos_thd_switch(newthd_cap);
	if(ret) printc("cos_thd_switch FAILED\n");

	/*
	 *  printc("\n------\nNew thread %d @ %x\n------\n\n",
	 * 		(int)newthd_cap,
	 * 		cos_introspect(&booter_info, newthd_cap, 0));
	 */
}


/* Called from RK init thread. The one in while(1) */

void
cos_resume()
{	
	thdid_t tid;
	int rcving;
	cycles_t cycles;
	int pending;

	while(1) {
		/* cos_rcv returns the number of pending messages */
		pending = cos_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &tid, &rcving, &cycles);
		//printc("cos_resume, pending:%d, tid:%d, rcving:%d\n", pending, tid, rcving);

		/* Handle all possible interrupts */
		if(tid && (!intr_getdisabled(cos_isr)) ) intr_pending(pending, tid, rcving);

		cos_thd_switch(cos_cur);
	}

	//if (cos_isr > 0)
	//	cos_thd_switch(cos_isr);
	//else
	//	cos_thd_switch(cos_cur);
}

void
cos_cpu_sched_switch(struct bmk_thread *unsused, struct bmk_thread *next)
{
	int ret;
	//capid_t tmp;
	//struct thd_creation_protocol info;
	//struct thd_creation_protocol *thd_meta = &info;

	/* FIXME
	 * RG: May, or may not need locks. Test 
         */
	//lock(&lk);
	//printc("\tTook lock: switch\n");
	cos_cur = get_cos_thdcap(next);
	//tmp = cos_cur;
	//unlock(&lk);
	//printc("\tReleasing lock: switch\n");

	//thd_meta->retcap = get_cos_thdcap(next);

	/* For Debugging
	 *
	 *printc("------\nSwitching thread to %d @ %x\n------\n",
	 *     	(int)(thd_meta->retcap),
	 *     	cos_introspect(&booter_info, thd_meta->retcap, 0));
	 */
	 

	//printc("\nprev: %s\n", get_name(prev));
	//printc("next: %s\n\n", get_name(next));
	//printc("retcap: %d\n\n", thd_meta->retcap);

	//ret = cos_thd_switch(thd_meta->retcap);
	ret = cos_thd_switch(cos_cur);
	if(ret)
		printc("thread switch failed\n");
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

	//curtime = (long long)(tsc_now / cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE)); /* cycles to us */
	curtime = (long long)(tsc_now / cycles_us); /* cycles to us */
	curtime = (long long)(curtime * 1000); /* us to ns */

	return curtime;
}

void *
cos_vatpa(void * vaddr)
{
//        int paddr = call_cap_op(BOOT_CAPTBL_SELF_PT, CAPTBL_OP_INTROSPECT, (int)vaddr, 0,0,0);
//	paddr = (paddr & 0xfffff000) | ((int)vaddr & 0x00000fff);
//        return (void *)paddr;
	return cos_va2pa(&booter_info, vaddr);
}

void *
cos_pa2va(void * pa, unsigned long len) 
{
        return (void *)cos_hw_map(&booter_info, BOOT_CAPTBL_SELF_INITHW_BASE, (paddr_t)pa, (unsigned int)len);
}

void
cos_vm_exit(void)
{ while (1) cos_thd_switch(VM_CAPTBL_SELF_EXITTHD_BASE); }
