#ifndef COS_INIT_H
#define COS_INIT_H

void cos_run(char *cmdline);

/* These are defiend on the rumpkernel side
 * Think of these as upcalls to the rump kernel to start
 * the rump kernel's main thread
 */
void bmk_sched_startmain(void (*)(void *), void *) __attribute__((noreturn));
void bmk_mainthread(void *);
void bmk_memalloc_init(void);
void bmk_pgalloc_loadmem(unsigned long min, unsigned long max);
void bmk_sched_init(void);
int  bmk_intr_init(void);

#endif
