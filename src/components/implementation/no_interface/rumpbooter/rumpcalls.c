#include <stdio.h>
#include <string.h>
#include <cos_component.h>
#include <cos_alloc.h>
#include <cos_kernel_api.h>
#include <cos_types.h>

#include "rumpcalls.h"
#include "rump_cos_alloc.h"
#include "cos_sched.h"
//#include "cos_lock.h"
#include "vkern_api.h"
//#define FP_CHECK(void(*a)()) ( (a == null) ? printc("SCHED: ERROR, function pointer is null.>>>>>>>>>>>\n");: printc("nothing");)
#include "cos_sync.h"

extern struct cos_compinfo booter_info;
extern struct cos_rumpcalls crcalls;

/* Thread cap */
volatile thdcap_t cos_cur = BOOT_CAPTBL_SELF_INITTHD_BASE;

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
	crcalls.rump_sched_yield		= cos_sched_yield;

	crcalls.rump_shmem_send			= cos_shmem_send;
	crcalls.rump_shmem_recv			= cos_shmem_recv;
	crcalls.rump_dequeue_size		= cos_dequeue_size;

	return;
}

int
cos_dequeue_size(unsigned int srcvm, unsigned int curvm)
{
	return vk_dequeue_size(srcvm, curvm);
}

/*rk shared mem functions*/
int
cos_shmem_send(void * buff, unsigned int size, unsigned int srcvm, unsigned int dstvm){

	asndcap_t sndcap;

	if(srcvm == 0) sndcap = VM0_CAPTBL_SELF_IOASND_SET_BASE + (dstvm - 1) * CAP64B_IDSZ;
	else sndcap = VM_CAPTBL_SELF_IOASND_BASE;

	cos_shm_write(buff, size, srcvm, dstvm);	
	cos_asnd(sndcap);	
	return 1;
}

int
cos_shmem_recv(void * buff, unsigned int srcvm, unsigned int curvm){
	return cos_shm_read(buff, srcvm, curvm);
}

/* send and recieve notifications */
void
rump2cos_rcv(void)
{
	printc("rump2cos_rcv");	
	return;
}

/* irq */
void
cos_irqthd_handler(void *line)
{
//	printc("cos_irqthd_handler\n");
	int which = (int)line;
	
	while(1) {
		int pending = cos_rcv(irq_arcvcap[which]);

		intr_start(irq_thdcap[which]);

		bmk_isr(which);

		intr_end();
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

	newthd_cap = cos_thd_alloc(&booter_info, booter_info.comp_cap, f, arg);
	assert(newthd_cap);
	set_cos_thddata(thread, newthd_cap, cos_introspect(&booter_info, newthd_cap, 9));
}

extern int vmid;

static inline void
intr_switch(void)
{
	int ret, i = 32;

	if(!intrs) return;

	/* Man this is ugly...FIXME */
	for(; i > 0 ; i--) {
		int tmp = intrs;

		if((tmp>>(i-1)) & 1) {
			do {
				/* VM1  to DOM0 */
				if(i == 1) {
					//printc("VM1 to DOM0, context VM%d intr\n", vmid);
					ret = cos_switch(VM0_CAPTBL_SELF_IOTHD_SET_BASE + 0*CAP_SZ_16B, 0, 0, 0, BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());
				}
				/* DOM0 to VM1 */
				else if(i == 2) {
					//printc("DOM0 to VM1 intr, context VM%d\n", vmid);
					 ret = cos_switch(VM_CAPTBL_SELF_IOTHD_BASE, 0, 0, 0, BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());
				}
				/* VM2  to DOM0*/
				else if(i == 7) {
					//printc("VM2 to DOM0 intr, context VM%d\n", vmid);
					ret = cos_switch(VM0_CAPTBL_SELF_IOTHD_SET_BASE + 1*CAP_SZ_16B, 0, 0, 0, BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());
				}
				/* DOM0 to VM2 */
				else if(i == 8) {
					//printc("DOM0 to VM2 intr, context VM%d\n", vmid);
					ret = cos_switch(VM_CAPTBL_SELF_IOTHD_BASE, 0, 0, 0, BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());
				}
				else {
					ret = cos_switch(irq_thdcap[i], 0, 0, 0, BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());
				}
				assert (ret == 0 || ret == -EAGAIN);
			} while (ret == -EAGAIN);
		}
	}
}


/* Called once from RK init thread. The one in while(1) */
void
cos_resume(void)
{
	while(1) {
		int ret, first = 1;
		unsigned int isdisabled;

		do {
			thdcap_t contending;
			cycles_t cycles;
			int pending, tid, rcving, irq_line;

			/*
			 * Handle all possible interrupts when
			 * interrupts are enabled or when
			 * a cos interrupt thread has disabled interrupts.
			 * Otherwise a rk thread disabled them and we need to
			 * switch back so it can enable interrupts
			 *
			 * Loop is neccessary incase we get preempted before a valid
			 * interrupt finishes execuing and we requrie that it finishes
			 * executing before returning to RK
		 	 */

			do {
				pending = cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &tid, &rcving, &cycles);
				assert(pending <= 1);

				irq_line = intr_translate_thdid2irq(tid);
				intr_update(irq_line, rcving);

				if(first) {
					isr_get(cos_isr, &isdisabled, &contending);
					if(isdisabled && !cos_intrdisabled) goto rk_resume;
					first = 0;
				}
			} while(pending);

			/*
			 * Done processing pending events
			 * Finish any remaining interrupts
			 */
			intr_switch();

		} while(intrs);

		assert(!intrs);

rk_resume:
		do {
			if(cos_intrdisabled) break;
			ret = cos_switch(cos_cur, 0, 0, 0, BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());
			assert(ret == 0 || ret == -EAGAIN);
		} while(ret == -EAGAIN);
	}
}

void
cos_cpu_sched_switch(struct bmk_thread *unsused, struct bmk_thread *next)
{
	sched_tok_t tok = cos_sched_sync();
	thdcap_t temp   = get_cos_thdcap(next);
	int ret;

	if(intrs) printc("FIXME: An interrupt is pending while rk is switching threads...\n");
	if(cos_isr) printc("%b\n", cos_isr);
	assert(!cos_isr);
	cos_cur = temp;

	do {
		ret = cos_switch(cos_cur, 0, 0, 0, BOOT_CAPTBL_SELF_INITRCV_BASE, tok);
		assert(ret == 0 || ret == -EAGAIN);
		if (ret == -EAGAIN) {
			/*
			 * I was preempted after getting the token and before updating cos_cur which just outdated my sched token
			 * So get a new token and try cos_switch again
			 * 
			 * And cos_cur == temp, can only happen if I've updated cos_cur and there were no other RK threads switched-to after that.
			 */
			if (cos_cur == temp) tok = cos_sched_sync();
			/*
			 * cos_cur is set to 'me' by some other RK thread because I was preempted after updating cos_cur
			 * ignore -EAGAIN in this scenario
			 */
			else break;
		}
	} while (ret == -EAGAIN);
}

/* --------- Timer ----------- */

/* Get the number of cycles in a single micro second. If we do nano second we lose the fraction */
long long cycles_us = (long long)(CPU_GHZ * 1000);

/* Return monotonic time since RK initiation in nanoseconds */
long long
cos_cpu_clock_now(void)
{
	uint64_t tsc_now = 0;
	unsigned long long curtime = 0;

	rdtscll(tsc_now);

	/* We divide as we have cycles and cycles per micro second */
	curtime = (long long)(tsc_now / cycles_us); /* cycles to micro seconds */
	curtime = (long long)(curtime * 1000); /* micro to nano seconds */

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

void
cos_sched_yield(void)
{ cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE); }
