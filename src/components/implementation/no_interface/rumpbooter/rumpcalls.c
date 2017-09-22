#include <stdio.h>
#include <string.h>
#include <cos_component.h>
#include <cos_alloc.h>
#include <cos_kernel_api.h>
#include <cos_types.h>
#include <cos_asm_simple_stacks.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <sl_thd.h>

#include "vk_types.h"
#include "rumpcalls.h"
#include "rump_cos_alloc.h"
//#include "cos_lock.h"
#include "cos2rk_rb_api.h"
//#define FP_CHECK(void(*a)()) ( (a == null) ? printc("SCHED: ERROR, function pointer is null.>>>>>>>>>>>\n");: printc("nothing");)
#include "vk_api.h"
#include "rk_inv_api.h"
#include "rk_sched.h"

extern int vmid;
extern struct cos_compinfo *currci;
extern struct cos_rumpcalls crcalls;

/* Thread cap */
volatile thdcap_t cos_cur = BOOT_CAPTBL_SELF_INITTHD_BASE;
volatile unsigned int cos_cur_tcap = BOOT_CAPTBL_SELF_INITTCAP_BASE;

tcap_prio_t rk_thd_prio = PRIO_MID;

/* Mapping the functions from rumpkernel to composite */
void
cos2rump_setup(void)
{
	rump_bmk_memsize_init();

	crcalls.rump_cpu_clock_now		= cos_cpu_clock_now;
	crcalls.rump_vm_clock_now		= cos_vm_clock_now;
	crcalls.rump_cos_print			= cos_print;
	crcalls.rump_vsnprintf			= vsnprintf;
	crcalls.rump_strcmp			= strcmp;
	crcalls.rump_strncpy			= strncpy;

	/* These should be removed, confirm that they are never used */
	crcalls.rump_memcalloc			= cos_memcalloc;
	crcalls.rump_memalloc			= cos_memalloc;


	crcalls.rump_cos_thdid			= cos_thdid;
	crcalls.rump_memcpy			= memcpy;
	crcalls.rump_memset			= (void *)cos_memset;
	crcalls.rump_cpu_sched_create		= cos_cpu_sched_create;

	if(!crcalls.rump_cpu_sched_create) printc("SCHED: rump_cpu_sched_create is set to null");

	crcalls.rump_cpu_sched_switch_viathd    = rk_rump_thd_yield_to;
	crcalls.rump_memfree			= cos_memfree;
	crcalls.rump_tls_init			= cos_tls_init;
	crcalls.rump_va2pa			= cos_vatpa;
	crcalls.rump_pa2va			= cos_pa2va;
	crcalls.rump_resume                     = rk_sched_loop;
	crcalls.rump_platform_exit		= cos_vm_exit;

	crcalls.rump_intr_enable		= rk_intr_enable;
	crcalls.rump_intr_disable		= rk_intr_disable;
	crcalls.rump_sched_yield		= cos_sched_yield;
	crcalls.rump_vm_yield			= cos_vm_yield;

	crcalls.rump_cpu_intr_ack		= cos_cpu_intr_ack;

	crcalls.rump_shmem_send			= cos_shmem_send;
	crcalls.rump_shmem_recv			= cos_shmem_recv;
	crcalls.rump_dequeue_size		= cos_dequeue_size;

	crcalls.rump_cpu_sched_wakeup		= rk_rump_thd_wakeup;
	crcalls.rump_cpu_sched_block_timeout	= rk_rump_thd_block_timeout;
	crcalls.rump_cpu_sched_block		= rk_rump_thd_block;
	crcalls.rump_cpu_sched_yield		= rk_rump_thd_yield;
	crcalls.rump_cpu_sched_exit		= rk_rump_thd_exit;

	return;
}

#define STR_LEN_MAX 127
static int slen = -1;
static char str[STR_LEN_MAX + 1];

static inline void
__reset_str(void)
{
	memset(str, 0, STR_LEN_MAX + 1);
	slen = 0;
}

void
cos_printflush(void)
{
	if (slen > 0) {
		cos_print(str, slen);
		__reset_str();
	}
}

/* last few chars still in buffer */
void
cos_vm_print(char s[], int ret)
{
	int len = 0, rem = ret;

	assert(ret <= STR_LEN_MAX+1);
	if (slen == -1) __reset_str();;

	if (slen + rem > STR_LEN_MAX) {
		len = STR_LEN_MAX - slen;
		rem = ret - len;
		strncpy(str+slen, s, len);
		slen += len;
		cos_print(str, slen);

		__reset_str();
	}

	strncpy(str+slen, s+len, rem);
	slen += rem;
	assert(slen <= STR_LEN_MAX);
}

int
cos_dequeue_size(unsigned int srcvm, unsigned int curvm)
{
	assert(0);
	return cos2rk_dequeue_size(srcvm, curvm);
}

/*rk shared mem functions*/
int
cos_shmem_send(void * buff, unsigned int size, unsigned int srcvm, unsigned int dstvm)
{
	asndcap_t sndcap;
	int ret;

	assert(0);
	return 1;
}

int
cos_shmem_recv(void * buff, unsigned int srcvm, unsigned int curvm)
{
	assert(0);
	return cos2rk_shm_read(buff, srcvm, curvm);
}

/* send and recieve notifications */
void
rump2cos_rcv(void)
{
	printc("rump2cos_rcv");
	return;
}

static inline void
__cpu_intr_ack(void)
{
	if (vmid) return;

	__asm__ __volatile(
		"movb $0x20, %%al\n"
		"outb %%al, $0xa0\n"
		"outb %%al, $0x20\n"
		::: "al");
}

void
cos_cpu_intr_ack(void)
{
	__cpu_intr_ack();
}

/* irq */
void
cos_irqthd_handler(arcvcap_t rcvc, void *line)
{
	int which = (int)line;

	printc("=[%d]", which);
	while(1) {
		int rcvd = 0;

		/*
		 * For N/w INT, Data is available on DMA and doesn't need
		 * multiple queuing of events to process all data (if there are multiple events pending)
		 */
		cos_rcv(rcvc, RCV_ALL_PENDING, &rcvd);

		/*
		 * This only wakes up isr_thread. 
		 * Now, using sl_thd_wakeup. So, don't need to disable interrupts around this!
		 */
		bmk_isr(which);
	}
}

/* Memory */
extern unsigned long bmk_memsize;
void
rump_bmk_memsize_init(void)
{
	/* (1<<20) == 1 MG */
	bmk_memsize = COS2RK_VIRT_MACH_MEM_SZ(vmid) - ((1<<20)*2);
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

int
cos_tls_init(unsigned long tp, thdcap_t tc)
{
	return cos_thd_mod(currci, tc, (void *)tp);
}


void
cos_cpu_sched_create(struct bmk_thread *thread, struct bmk_tcb *tcb,
		void (*f)(void *), void *arg,
		void *stack_base, unsigned long stack_size)
{
	struct sl_thd *t = NULL;
	struct cos_aep_info tmpaep;
	int ret;

	printc("cos_cpu_sched_create: thread->bt_name = %s, f: %p", thread->bt_name, f);

	if (!strcmp(thread->bt_name, "user_lwp")) {
		tmpaep.thd = VM_CAPTBL_SELF_APPTHD_BASE;
		tmpaep.rcv = 0;
		tmpaep.tc  = BOOT_CAPTBL_SELF_INITTCAP_BASE;
		t = rk_rump_thd_init(&tmpaep);
		assert(t);
		/* Return userlevel thread cap that is set up in vkernel_init */
		printc("\nMatch, thdcap %d, id:%u\n", (unsigned int)VM_CAPTBL_SELF_APPTHD_BASE, t->thdid);
	} else {
		t = rk_rump_thd_alloc(f, arg);
		assert(t);
		printc(" thdcap: %lu, id:%u\n", sl_thd_thdcap(t), t->thdid);
	}

	set_cos_thddata(thread, sl_thd_thdcap(t), t->thdid);
}

/* Return monotonic time since RK per VM initiation in nanoseconds */
extern u64_t t_vm_cycs;
extern u64_t t_dom_cycs;
long long
cos_vm_clock_now(void)
{
	u64_t tsc_now = 0;
	unsigned long long curtime = 0;

	assert(vmid <= 1);
	if (vmid == 0)      tsc_now = t_dom_cycs;
	else if (vmid == 1) tsc_now = t_vm_cycs;

	curtime = (long long)(tsc_now / cycs_per_usec); /* cycles to micro seconds */
        curtime = (long long)(curtime * 1000); /* micro to nano seconds */

	assert(cos_spdid_get() <= 1);
	if (cos_spdid_get() == 0)      tsc_now = t_dom_cycs;
	else if (cos_spdid_get() == 1) tsc_now = t_vm_cycs;

	curtime = (long long)(tsc_now / cycs_per_usec); /* cycles to micro seconds */
	curtime = (long long)(curtime * 1000); /* micro to nano seconds */

	return curtime;
}

/* Return monotonic time since RK initiation in nanoseconds */
long long
cos_cpu_clock_now(void)
{
	u64_t tsc_now = 0;
	unsigned long long curtime = 0;
        rdtscll(tsc_now);

	/* We divide as we have cycles and cycles per micro second */
        curtime = (long long)(tsc_now / cycs_per_usec); /* cycles to micro seconds */
        curtime = (long long)(curtime * 1000); /* micro to nano seconds */


	return curtime;
}

void *
cos_vatpa(void * vaddr)
{ return cos_va2pa(currci, vaddr); }

void *
cos_pa2va(void * pa, unsigned long len)
{ return (void *)cos_hw_map(currci, BOOT_CAPTBL_SELF_INITHW_BASE, (paddr_t)pa, (unsigned int)len); }

void
cos_vm_exit(void)
{ vk_vm_exit(); }

void
cos_sched_yield(void)
{ cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE); }

void
cos_vm_yield(void)
{ cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE); }

/* System Calls */
void
cos_fs_test(void)
{
	int sinv_ret;

	printc("Running cos fs test: VM%d\n", cos_spdid_get());

	sinv_ret = rk_inv_op1();

	printc("Ret from fs test: %d\n", sinv_ret);
}

/* FIXME rename two tests below */
void
cos_shmem_test(void)
{
	sinvcap_t sinv;
	int shm_id;
	vaddr_t my_page;

	/* Test is from User component to Kernel Component only */
	assert(cos_spdid_get() == 1);

	/* Allocate user component a shared page to use */
	shm_id = shmem_allocate_invoke();

	/* Get vaddr for that shm_id and write 'a' to it */
	my_page = shmem_get_vaddr_invoke(shm_id);
	assert(my_page);
	printc("User component shared mem vaddr: %p\n", (void *)my_page);
	*(char *)my_page = 'a';

	/* Go to kernel, map in shared mem page, read 'a', write 'b' */
	rk_inv_op2(shm_id);

	/* Read 'b' in our page */
	printc("Return from kernel component, reading %p + 1: %c\n", \
		(void *)my_page, *((char *)my_page + 1));
}

vaddr_t
shmem_get_vaddr_invoke(int id)
{
	return vk_shmem_vaddr_get(cos_spdid_get(), id); /* FIXME: is this right? */
}

int
shmem_allocate_invoke(void)
{
	return vk_shmem_alloc(cos_spdid_get(), 1);;
}

int
shmem_deallocate_invoke(void)
{
	return vk_shmem_dealloc();
}

int
shmem_map_invoke(int id)
{
	return vk_shmem_map(cos_spdid_get(), id);
}

int _spdid = -1;
void
cos_spdid_set(unsigned int spdid)
{
	/* Try and have some sort of sanity check that it is only being set once... */
	assert(_spdid < 0);

	_spdid = spdid;
}

unsigned int
cos_spdid_get(void)
{ return _spdid; }
