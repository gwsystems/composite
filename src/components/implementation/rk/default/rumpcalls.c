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
#include <rumpcalls.h>
#include <vk_types.h>
#include <vk_api.h>
#include <memmgr.h>
#include <capmgr.h>

#include "rump_cos_alloc.h"
#include "rk_sched.h"

extern int vmid;
extern struct cos_compinfo *currci;
extern struct cos_rumpcalls crcalls;
void __cos_print(char *s, int len);
extern spdid_t rk_child_app;

/* Mapping the functions from rumpkernel to composite */
void
cos2rump_setup(void)
{
	rump_bmk_memsize_init();

	crcalls.rump_cpu_clock_now		= cos_cpu_clock_now;
	crcalls.rump_vm_clock_now		= cos_vm_clock_now;
	crcalls.rump_cos_print			= __cos_print;
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

	if(!crcalls.rump_cpu_sched_create) PRINTC("SCHED: rump_cpu_sched_create is set to null");

	crcalls.rump_cpu_sched_switch_viathd    = rk_rump_thd_yield_to;
	crcalls.rump_memfree			= cos_memfree;
	crcalls.rump_tls_init			= cos_tls_init;
	crcalls.rump_tls_alloc			= cos_tls_alloc;
	crcalls.rump_tls_fetch			= cos_tls_fetch;
	crcalls.rump_va2pa			= cos_vatpa;
	crcalls.rump_pa2va			= cos_pa2va;
	crcalls.rump_resume                     = rk_sched_loop;
	crcalls.rump_platform_exit		= cos_vm_exit;

	crcalls.rump_intr_enable		= rk_intr_enable;
	crcalls.rump_intr_disable		= rk_intr_disable;
	crcalls.rump_sched_yield		= cos_sched_yield;
	crcalls.rump_vm_yield			= cos_vm_yield;

	crcalls.rump_cpu_intr_ack		= cos_cpu_intr_ack;

	crcalls.rump_cpu_sched_wakeup		= rk_rump_thd_wakeup;
	crcalls.rump_cpu_sched_block_timeout	= rk_rump_thd_block_timeout;
	crcalls.rump_cpu_sched_block		= rk_rump_thd_block;
	crcalls.rump_cpu_sched_yield		= rk_rump_thd_yield;
	crcalls.rump_cpu_sched_exit		= rk_rump_thd_exit;
	crcalls.rump_cpu_sched_set_prio		= rk_curr_thd_set_prio;

	return;
}

#define STR_LEN_MAX 127
static int slen = -1;
static char str[STR_LEN_MAX + 1];

extern cycles_t cycs_per_usec;

void
__cos_print(char *s, int len)
{
	if (len > 1) printc("(%d): ", cos_thdid());
	cos_print(s, len);
}

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

static inline void
__cpu_intr_ack(void)
{
//	static int count;

	__asm__ __volatile(
		"movb $0x20, %%al\n"
		"outb %%al, $0xa0\n"
		"outb %%al, $0x20\n"
		::: "al");

//	count ++;
//	if (count % 1000 == 0) printc("..a%d..", count);
}

void
cos_cpu_intr_ack(void)
{ __cpu_intr_ack(); }

/* irq */
void
cos_irqthd_handler(arcvcap_t rcvc, void *line)
{
	int which = (int)line;

	printc("=[%d]", which);
	while(1) {
		int rcvd = 0;

		/*
		 * TODO: for optimization!
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
	bmk_memsize = VM_UNTYPED_SIZE(vmid) - ((1<<20)*2);
	PRINTC("bmk_memsize: %lu\n", bmk_memsize);
}

void
cos_memfree(void *cp)
{
	rump_cos_free(cp);
}

void *
cos_memcalloc(size_t n, size_t size)
{

	PRINTC("cos_memcalloc was called\n");
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
	PRINTC("cos_memalloc was called\n");
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

extern int tcboffset;
extern int tdatasize;
extern int tbsssize;
extern const char *_tdata_start_cpy;

void *
cos_tls_alloc(struct bmk_thread *thread)
{
	char *tlsmem;

	tlsmem = memmgr_tls_alloc(thread->cos_tid);

	cos_memset((void *)(tlsmem + tdatasize), 0, tbsssize);

	return tlsmem + tcboffset;
}

void *
cos_tls_fetch(struct bmk_thread *thread)
{
	char *tlsmem;

	/*
	 * Doesn't allocate new memory, just returns the vaddr pointer to this threads tls region...
	 * Could be named better :P
	 */
	tlsmem = memmgr_tls_alloc(thread->cos_tid);

	return tlsmem + tcboffset;
}

void
cos_cpu_sched_create(struct bmk_thread *thread, struct bmk_tcb *tcb,
		void (*f)(void *), void *arg,
		void *stack_base, unsigned long stack_size)
{
	struct sl_thd *t = NULL;
	int ret;


	PRINTC("cos_cpu_sched_create: thread->bt_name = %s, f: %p, in spdid: %d\n", thread->bt_name, f,
	        cos_spdid_get());


	/* Check to see if we are creating the thread for our application */
	if (!strcmp(thread->bt_name, "user_lwp")) {
		int udpserver_id = rk_child_app;
		struct cos_defcompinfo tmpdci;

		cos_defcompinfo_childid_init(&tmpdci, udpserver_id);

		t = sl_thd_initaep_alloc(&tmpdci, NULL, 0, 0, 0, 0, 0);
		assert(t);
		sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, RK_RUMP_THD_PRIO));

	} else {
		t = rk_rump_thd_alloc(f, arg);
		assert(t);
	}

	PRINTC("new thread id: %d\n", sl_thd_thdid(t));
	set_cos_thddata(thread, sl_thd_thdcap(t), t->aepinfo->tid);
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
{ return (void *)memmgr_va2pa((vaddr_t)vaddr); }

void *
cos_pa2va(void * pa, unsigned long len)
{
	PRINTC("cos_pa2va\n");
	return (void *)memmgr_pa2va_map((paddr_t)pa, len);
}

void
cos_vm_exit(void)
{
	/* TODO this should be oen of the functions that rumpbooter interface exports when it becomes its own interface */
	PRINTC("current thread id: %d\n", sl_thd_thdid(sl_thd_curr()));
	//vk_vm_exit();
}

void
cos_sched_yield(void)
{ cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE); }

void
cos_vm_yield(void)
{ cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE); }


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
