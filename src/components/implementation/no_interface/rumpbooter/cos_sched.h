#ifndef COS_SCHED_H
#define COS_SCHED_H

int __attribute__((format(printf,1,2))) printc(char *fmt, ...);
void cos_cpu_sched_create(struct bmk_thread *thread, struct bmk_tcb *tcb,
		void (*f)(void *), void *arg,
		void *stack_base, unsigned long stack_size);
void cos_cpu_sched_switch(struct bmk_thread *prev, struct bmk_thread *next);
void cos_tls_init(unsigned long tp, thdcap_t tc);
void cos_resume(void);

#endif
