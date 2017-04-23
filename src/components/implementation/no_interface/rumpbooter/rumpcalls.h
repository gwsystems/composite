#ifndef RUMPCALLS_H
#define RUMPCALLS_H

#include "../../../../kernel/include/shared/consts.h"
#include "../../../../kernel/include/shared/cos_types.h"
#include <consts.h>
#include <cos_types.h>
#include "vk_types.h"

typedef __builtin_va_list va_list;
#undef va_start
#undef va_arg
#undef va_end
#define va_start(v,l)      __builtin_va_start((v),l)
#define va_arg             __builtin_va_arg
#define va_end(va_arg)     __builtin_va_end(va_arg)

#define IRQ_DOM0_VM 22 /* DOM0's message line to VM's, in VM's */
#define IRQ_VM1 21     /* VM1's message line to DOM0, so in DOM0 */
#define IRQ_VM2 27     /* VM2's message line to DOM0, so in DOM0 */
#define IRQ_DL 32     /* DLVMs IRQ line*/

extern struct cos_rumpcalls crcalls;

extern int boot_thd;

struct bmk_thread;
extern __thread struct bmk_thread *bmk_current;
extern tcap_prio_t rk_thd_prio;
extern volatile unsigned int cos_cur_tcap;
#define COS_CUR_TCAP ((tcap_t)((cos_cur_tcap << 16) >> 16))

struct bmk_tcb *tcb;
extern void bmk_isr(int which);

void* rump_cos_malloc(size_t size);
void* rump_cos_calloc(size_t nmemb, size_t _size);
void rump_cos_free(void *ptr);

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
	void   (*rump_tls_init)(unsigned long tp, capid_t tc); /* thdcap_t == capid_t*/
	void*  (*rump_va2pa)(void *addr);
	void*  (*rump_pa2va)(void *addr, unsigned long len);
	void   (*rump_resume)(void);
	void   (*rump_platform_exit)(void);
	void   (*rump_rcv)(void);
	void   (*rump_intr_disable)(void);
	void   (*rump_intr_enable)(void);
	int    (*rump_shmem_send)(void * buff, unsigned int size, unsigned int srcvm, unsigned int dstvm);
	int    (*rump_shmem_recv)(void * buff, unsigned int srcvm, unsigned int dstvm);
	void   (*rump_sched_yield)(void);
	void   (*rump_vm_yield)(void);
	void   (*rump_cpu_intr_ack)(void);
	int    (*rump_dequeue_size)(unsigned int srcvm, unsigned int dstvm);
};

/* Mapping the functions from rumpkernel to composite */
void cos2rump_setup(void);

void *cos_memcalloc(size_t n, size_t size);
void *cos_memalloc(size_t nbytes, size_t align);
void cos_memfree(void *cp);

void  rump_bmk_memsize_init(void);

void cos_cpu_intr_ack(void);
void set_cos_thddata(struct bmk_thread *thread, capid_t thd, thdid_t tid);
capid_t get_cos_thdcap(struct bmk_thread *thread);
thdid_t get_cos_thdid(struct bmk_thread *thread);
long long bmk_runq_empty(void);

char *get_name(struct bmk_thread *thread);
long long cos_cpu_clock_now(void);
long long cos_vm_clock_now(void);

void cos_irqthd_handler(void *line);

void *cos_vatpa(void* addr);
void *cos_pa2va(void* addr, unsigned long len);

void cos_vm_exit(void);
void cos_sched_yield(void);
void cos_vm_yield(void);
void cos_vm_print(char s[], int ret);

int cos_shmem_send(void * buff, unsigned int size, unsigned int srcvm, unsigned int dstvm);
int cos_shmem_recv(void * buff, unsigned int srcvm, unsigned int curvm);
int cos_dequeue_size(unsigned int srcvm, unsigned int curvm);

void cos_dom02io_transfer(unsigned int irqline, tcap_t tc, arcvcap_t rc, tcap_prio_t prio);
void cos_vio_tcap_update(unsigned int dst);
void cos_vio_tcap_set(unsigned int src);
tcap_t cos_find_vio_tcap(void);

#endif /* RUMPCALLS_H */
