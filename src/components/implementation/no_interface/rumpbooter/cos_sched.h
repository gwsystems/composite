#ifndef COS_SCHED_H
#define COS_SCHED_H

int __attribute__((format(printf,1,2))) printc(char *fmt, ...);
void cos_cpu_sched_create(struct bmk_thread *thread, struct bmk_tcb *tcb,
		void (*f)(void *), void *arg,
		void *stack_base, unsigned long stack_size);
void rump_thd_fn(void *param);
void cos_cpu_sched_switch(struct bmk_thread *prev, struct bmk_thread *next);
void cos_tls_init(unsigned long tp, thdcap_t tc);

struct thd_creation_protocol {
	unsigned short int *thdid;
	unsigned short int  retcap;
	void (*f)(void *);
	void *arg;
};

/* Fetches the cos_thdid so that we know how far into bmk_threads[] to index*/
void
rump_thd_fn(void *param)
{
	struct thd_creation_protocol *p = param;
	unsigned short int  thdid;
	unsigned short int retcap = p->retcap;
	void *arg   = p->arg;
	void (*f)(void *) = p->f;

	p->thdid = &thdid;

	thdid = cos_thdid();
	//printc("thdid: %d\n", thdid);

	cos_thd_switch(retcap);
	f(arg);
}

#endif
