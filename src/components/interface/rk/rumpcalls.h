#ifndef RUMPCALLS_H
#define RUMPCALLS_H

#include <consts.h>
#include <cos_types.h>
#include <stdio.h>

typedef __builtin_va_list va_list;
#undef va_start
#undef va_arg
#undef va_end
#define va_start(v,l)      __builtin_va_start((v),l)
#define va_arg             __builtin_va_arg
#define va_end(va_arg)     __builtin_va_end(va_arg)

#define NAME_MAXLEN 16
#undef _TAILQ_ENTRY
#undef TAILQ_ENTRY
#define _TAILQ_ENTRY(type, qual)                                        \
struct {                                                                \
        qual type *tqe_next;            /* next element */              \
        qual type *qual *tqe_prev;      /* address of previous next element */\
}
#define TAILQ_ENTRY(type) _TAILQ_ENTRY(struct type,)

extern __thread struct bmk_thread *bmk_current;
struct bmk_tcb *tcb;
struct bmk_tcb {
        size_t btcb_sp;          /* stack pointer        */
        size_t btcb_ip;          /* program counter      */

        size_t btcb_tp;          /* tls pointer          */
	size_t btcb_tpsize; /* tls area length      */
};

struct bmk_thread {
        char bt_name[NAME_MAXLEN];

        long long bt_wakeup_time;

        int bt_flags;
        int bt_errno;

        void *bt_stackbase;

        void *bt_cookie;

        /* MD thread control block */
        struct bmk_tcb bt_tcb;

        TAILQ_ENTRY(bmk_thread) bt_schedq;
        TAILQ_ENTRY(bmk_thread) bt_threadq;

	capid_t cos_thdcap;
	thdid_t cos_tid;
	long long runtime_start;
	long long runtime_end;
	int firsttime;
};

/*
 * These are defined on the rumpkernel side
 * Think of these as upcalls to the rump kernel
 */
extern void bmk_isr(int which);
void bmk_sched_startmain(void (*)(void *), void *) __attribute__((noreturn));
void bmk_mainthread(void *);
void bmk_memalloc_init(void);
void bmk_pgalloc_loadmem(unsigned long min, unsigned long max);
void bmk_sched_init(void);
int  bmk_intr_init(void);

extern int boot_thd;
extern struct cos_rumpcalls crcalls;
extern tcap_prio_t rk_thd_prio;
extern volatile unsigned int cos_cur_tcap;
#define COS_CUR_TCAP ((tcap_t)((cos_cur_tcap << 16) >> 16))

struct cos_rumpcalls
{
	unsigned short int (*rump_cos_get_thd_id)(void);
	long long (*rump_cpu_clock_now)(void);
	long long (*rump_vm_clock_now)(void);
	int    (*rump_vsnprintf)(char* str, size_t size, const char *format, va_list arg_ptr);
	void   (*rump_cos_print)(char s[], int ret);
	int    (*rump_strcmp)(const char *a, const char *b);
	char*  (*rump_strncpy)(char *d, const char *s, size_t n);
	void*  (*rump_memcalloc)(size_t n, size_t size);
	void*  (*rump_memalloc)(size_t nbytes, size_t align);
	void*  (*rump_pgalloc)(void);
	void   (*rump_memfree)(void *cp);
	void*  (*rump_memset)(void *b, int c, size_t n); //testing
	u16_t  (*rump_cos_thdid)(void);
	void*  (*rump_memcpy)(void *d, const void *src, size_t n);
	void   (*rump_cpu_sched_create)(struct bmk_thread *thread, struct bmk_tcb *tcb,
			void (*f)(void *), void *arg,
			void *stack_base, unsigned long stack_size);
	void   (*rump_cpu_sched_switch_viathd)(struct bmk_thread *prev, struct bmk_thread *next);
	int    (*rump_tls_init)(unsigned long tp, capid_t tc); /* thdcap_t == capid_t*/
	void*  (*rump_tls_alloc)(struct bmk_thread *thread);
	void*  (*rump_tls_fetch)(struct bmk_thread *thread);
	void*  (*rump_va2pa)(void *addr);
	void*  (*rump_pa2va)(void *addr, unsigned long len);
	void   (*rump_resume)(void);
	void   (*rump_platform_exit)(void);
	void   (*rump_rcv)(void);
	void   (*rump_intr_disable)(void);
	void   (*rump_intr_enable)(void);
	void   (*rump_sched_yield)(void);
	void   (*rump_vm_yield)(void);
	void   (*rump_cpu_intr_ack)(void);
	int    (*rump_dequeue_size)(unsigned int srcvm, unsigned int dstvm);

	void   (*rump_cpu_sched_wakeup)(struct bmk_thread *t);
	int    (*rump_cpu_sched_block_timeout)(struct bmk_thread *curr, unsigned long long timeout);
	void   (*rump_cpu_sched_block)(struct bmk_thread *curr);
	void   (*rump_cpu_sched_yield)(void);
	void   (*rump_cpu_sched_exit)(void);
	void   (*rump_cpu_sched_set_prio)(int prio);
};

void* rump_cos_malloc(size_t size);
void* rump_cos_calloc(size_t nmemb, size_t _size);
void rump_cos_free(void *ptr);

/* Mapping the functions from rumpkernel to composite */
void cos2rump_setup(void);

void *cos_memcalloc(size_t n, size_t size);
void *cos_memalloc(size_t nbytes, size_t align);
void cos_memfree(void *cp);

void  rump_bmk_memsize_init(void);

void set_cos_thddata(struct bmk_thread *thread, capid_t thd, thdid_t tid);
capid_t get_cos_thdcap(struct bmk_thread *thread);
thdid_t get_cos_thdid(struct bmk_thread *thread);
int fakethd_create(int instance, char *name);

char *get_name(struct bmk_thread *thread);
long long cos_cpu_clock_now(void);
long long cos_vm_clock_now(void);

void cos_irqthd_handler(arcvcap_t intrrc, void *line);

void *cos_vatpa(void* addr);
void *cos_pa2va(void* addr, unsigned long len);

void cos_vm_exit(void);
void cos_sched_yield(void);
void cos_vm_yield(void);

void cos_cpu_intr_ack(void);

vaddr_t shmem_get_vaddr_invoke(int id);
int shmem_allocate_invoke(void);
int shmem_deallocate_invoke(void);
int shmem_map_invoke(int id);

void cos_spdid_set(unsigned int spdid);
unsigned int cos_spdid_get(void);

int __attribute__((format(printf,1,2))) printc(char *fmt, ...);
void cos_cpu_sched_create(struct bmk_thread *thread, struct bmk_tcb *tcb,
                void (*f)(void *), void *arg,
                void *stack_base, unsigned long stack_size);
void cos_cpu_sched_switch(struct bmk_thread *prev, struct bmk_thread *next);
int cos_tls_init(unsigned long tp, thdcap_t tc);
void *cos_tls_alloc(struct bmk_thread *thread);
void *cos_tls_fetch(struct bmk_thread *thread);
void cos_resume(void);

#endif /* RUMPCALLS_H */
