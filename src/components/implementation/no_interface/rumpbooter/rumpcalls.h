#ifndef RUMPCALLS_H
#define RUMPCALLS_H

#include <consts.h>
#include <cos_types.h>

typedef __builtin_va_list va_list;
#define va_start(v,l)      __builtin_va_start((v),l)
#define va_arg             __builtin_va_arg
#define va_end(va_arg)     __builtin_va_end(va_arg)

extern struct cos_rumpcalls crcalls;
extern struct bmk_thread *bmk_threads[];
struct bmk_tcb *tcb;
#define bmk_current bmk_threads[crcalls.rump_cos_thdid()]

struct cos_rumpcalls
{
	unsigned short int (*rump_cos_get_thd_id)(void);
	int    (*rump_vsnprintf)(char* str, size_t size, const char *format, va_list arg_ptr);
	void   (*rump_cos_print)(char s[], int ret);
	int    (*rump_strcmp)(const char *a, const char *b);
	char*  (*rump_strncpy)(char *d, const char *s, unsigned long n);
	void*  (*rump_memcalloc)(unsigned long n, unsigned long size);
	void*  (*rump_memalloc)(unsigned long nbytes, unsigned long align);
	void*  (*rump_pgalloc)(void);
	void   (*rump_memfree)(void *cp);
	void   (*rump_memset)(void *b, int c, unsigned long n); //testing
	u16_t  (*rump_cos_thdid)(void);
	void*  (*rump_memcpy)(void *d, const void *src, unsigned long n);
	void   (*rump_cpu_sched_create)(struct bmk_thread *thread, struct bmk_tcb *tcb,
			void (*f)(void *), void *arg,
			void *stack_base, unsigned long stack_size);
	void   (*rump_cpu_sched_switch_viathd)(struct bmk_thread *prev, struct bmk_thread *next);
};

/* Mapping the functions from rumpkernel to composite */
void cos2rump_setup(void);

void *cos_memcalloc(size_t n, size_t size);
void *cos_memalloc(size_t nbytes, size_t align);
void cos_memfree(void *cp);

void  rump_bmk_memsize_init(void);

void set_cos_thdcap(struct bmk_thread *thread, capid_t value);
capid_t get_cos_thdcap(struct bmk_thread *thread);

#endif /* RUMPCALLS_H */
