#ifndef SLM_PRIVATE_H
#define SLM_PRIVATE_H

#include <cos_component.h>
#include <ps.h>
#include <slm_api.h>

typedef unsigned long slm_cs_cached_t;
/* Critical section (cs) API to protect scheduler data-structures */
struct slm_cs {
	unsigned long owner_contention;
};

static inline int
slm_state_is_runnable(slm_thd_state_t s)
{ return s == SLM_THD_RUNNABLE || s == SLM_THD_WOKEN; }

static inline int
slm_state_is_dead(slm_thd_state_t s)
{ return s == SLM_THD_FREE || s == SLM_THD_DYING; }

/**
 * Get the metadata about the critical section (CS). This includes the
 * owner thread (or `NULL`), and if the CS has been contended by another
 * thread.
 *
 * - @cs - the critical section
 * - @thd - the *returned* owner thread or `NULL`
 * - @contention - {`0`, `1`} depending on if there is contention
 * - @ret - a *cached* version of the critical section metadata to be passed into `__slm_cs_cas`.
 */
static inline slm_cs_cached_t
__slm_cs_data(struct slm_cs *cs, struct slm_thd **thd, int *contention)
{
	unsigned long oc = ps_load(&cs->owner_contention);
	/* least significant bit signifies contention */
	*thd        = (struct slm_thd *)(oc & (~0UL << 1));
	*contention = oc & 1;

	return oc;
}

/**
 * Update the critical section atomically using compare-and-swap.
 * `cached` was returned from `__slm_cs_data`.
 *
 * - @cs - the critical section
 * - @cached - the previous value of the critical section
 * - @thd - thread owning the critical section or NULL
 * - @contention - must be in {`0`, `1`} -- denotes if we want the
 *                 contention bit to be set.
 * - @ret - `0` on success to update the cs, `1` on failure
 */
static inline int
__slm_cs_cas(struct slm_cs *cs, slm_cs_cached_t cached, struct slm_thd *thd, int contention)
{
	return !ps_cas(&cs->owner_contention, (unsigned long)cached, ((unsigned long)thd | !!contention));
}

struct slm_global {
	struct slm_cs lock;

	struct slm_thd sched_thd;
	struct slm_thd idle_thd;

	int         cyc_per_usec;
	int         timer_set; 	  /* is the timer set? */
	cycles_t    timer_next;	  /* ...what is it set to? */
	tcap_time_t timeout_next; /* ...and what is the tcap representation? */

	struct ps_list_head event_head;     /* all pending events for sched end-point */
	struct ps_list_head graveyard_head; /* all deinitialized threads */

	struct cos_scb_info *scb;
} CACHE_ALIGNED;

/*
 * Simply retrieve this core's global data-structures.
 */
static inline struct slm_global *
slm_global(void)
{
	extern struct slm_global __slm_global[NUM_CPU];

	return &__slm_global[cos_coreid()];
}

static inline struct slm_global *
slm_global_core(int i)
{
	extern struct slm_global __slm_global[NUM_CPU];

	return &__slm_global[i];
}

static inline struct cos_scb_info *
slm_scb_info_test(int i)
{
	return (slm_global_core(i)->scb);
}


static inline struct cos_scb_info *
slm_scb_info_core(void)
{
	return (slm_global()->scb);
}

static inline struct cos_dcb_info *
slm_thd_dcbinfo(struct slm_thd *t)
{ return t->dcb; }

static inline unsigned long *
slm_thd_dcbinfo_ip(struct slm_thd *t)
{ return &t->dcb->ip; }

static inline unsigned long *
slm_thd_dcbinfo_sp(struct slm_thd *t)
{ return &t->dcb->sp; }


/*
 * Return if the given thread is normal, i.e. not the idle thread nor
 * the scheduler thread.
 */
static inline int
slm_thd_normal(struct slm_thd *t)
{
	struct slm_global *g = slm_global();

	return t != &g->idle_thd && t != &g->sched_thd;
}

/*
 * If the current thread is the scheduler or idle thread, return that
 * slm_thd. That thread should generally never be used
 */
struct slm_thd *slm_thd_special(void);

static inline int
cos_ulswitch(thdcap_t curr, thdcap_t next, struct cos_dcb_info *cd, struct cos_dcb_info *nd, tcap_prio_t prio, tcap_time_t timeout, sched_tok_t tok)
{
	volatile struct cos_scb_info *scb = slm_scb_info_core();
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_compinfo *ci = cos_compinfo_get(defci);
	struct cos_aep_info    *sched_aep = cos_sched_aep_get(defci);
	sched_tok_t rcv_tok;
	unsigned long pre_tok;

	assert(curr != next);
	/* 
	 * If previous timer smaller than current timeout, we need to take 
	 * the kernel path to update the timer.
	 */
	if (scb->timer_pre < timeout) {
		scb->timer_pre = timeout;
		return cos_defswitch(next, prio, timeout, tok);
	}
	/*
	 * jump labels in the asm routine:
	 *
	 * 1: slowpath dispatch using cos_thd_switch to switch to a thread
	 *    if the dcb sp of the next thread is reset.
	 *	(inlined slowpath sysenter to debug preemption problem)
	 *
	 * 2: if user-level dispatch routine completed successfully so
	 *    the register states still retained and in the dispatched thread
	 *    we reset its dcb sp!
	 *
	 * 3: if user-level dispatch was either preempted in the middle
	 *    of this routine or kernel at some point had to switch to a
	 *    thread that co-operatively switched away from this routine.
	 *    NOTE: kernel takes care of resetting dcb sp in this case!
	 */

	assert(timeout);

	/* __asm__ __volatile__ (           \
	 *	"pushl %%ebp\n\t"               \
	 *	"movl %%esp, %%ebp\n\t"         \
	 *	"movl $2f, (%%eax)\n\t"         \ save ip of the current thread, 
	                                      when switch back to this thread
										  it should continue from $2f.
	 *	"movl %%esp, 4(%%eax)\n\t"      \ save sp of the current thread,
	                                      if sp in the dcb is non-zero, 
										  meaning the ip is valid.
	 *	"cmp $0, 4(%%ebx)\n\t"          \ compare if dcb of the **next**
	                                      thread has a non-zero sp.
	 *	"je 1f\n\t"                     \ if sp of the dcb of the next
	                                      thread equals 0, we will take
										  kernel dispatch path.
	 *	"movl %%edx, (%%ecx)\n\t"       \ update current active thread
	                                      in the scb of the current core.
	 *	"movl 4(%%ebx), %%esp\n\t"      \ load the sp of the thread we
	                                      are dispatching to.
	 *	"jmp *(%%ebx)\n\t"              \ switch thread!
	 *	".align 4\n\t"                  \
	 *	"1:\n\t"                        \ this is the kernel path of
	                                      dispatching a thread. It should
										  mimic what we do in normal syscall.
	 *	"movl $3f, %%ecx\n\t"           \
	 *	"movl %%edx, %%eax\n\t"         \
	 *	"inc %%eax\n\t"                 \
	 *	"shl $16, %%eax\n\t"            \
	 *	"movl $0, %%ebx\n\t"            \
	 *	"movl $0, %%esi\n\t"            \
	 *	"movl %%edi, %%edx\n\t"         \
	 *	"movl $0, %%edi\n\t"            \
	 *	"sysenter\n\t"                  \
	 *	"jmp 3f\n\t"                    \
	 *	".align 4\n\t"                  \ The alignment is important, since
	                                      kernel rely on the alignment to
										  calculate which ip to return to
										  if a thread got preempted during
										  a user-level thread dispatch.
	 *	"2:\n\t"                        \
	 *	"movl $0, 4(%%ebx)\n\t"         \ Reset the value of the sp in the dcb
	                                      of the **current** thread (because)
										  we already switch to the "next"
										  thread
	 *	".align 4\n\t"                  \
	 *	"3:\n\t"                        \
	 *	"popl %%ebp\n\t"                \
	 *	: "=S" (htok)
	 *	: "a" (cd), "b" (nd),
	 *	  "S" (tok), "D" (timeout),
	 *	  "c" (&(scb->curr_thd)), "d" (next)
	 *	: "memory", "cc");
     */
#if defined(__x86_64__)
	__asm__ __volatile__ (
		"pushq %%rbp\n\t"               \
		"mov %%rsp, %%rbp\n\t"          \
		"movabs $2f, %%r8\n\t"          \
		"mov %%r8, (%%rax)\n\t"         \
		"mov %%rsp, 8(%%rax)\n\t"       \
		"cmp $0, 8(%%rbx)\n\t"          \
		"je 1f\n\t"                     \
		"mov %%rdx, (%%rcx)\n\t"        \
		"mov 8(%%rbx), %%rsp\n\t"       \
		"jmp *(%%rbx)\n\t"              \
		".align 4\n\t"                  \
		"1:\n\t"                        \
		"movq $3f, %%r8\n\t"            \
		"mov %%rdx, %%rax\n\t"          \
		"inc %%rax\n\t"                 \
		"shl $16, %%rax\n\t"            \
		"mov %%rsi, %%rbx\n\t"          \
		"mov $0, %%rsi\n\t"             \
		"mov %%rdi, %%rdx\n\t"          \
		"mov $0, %%rdi\n\t"             \
		"syscall\n\t"                   \
		"jmp 3f\n\t"                    \
		".align 4\n\t"                  \
		"2:\n\t"                        \
		"movq $0, 8(%%rbx)\n\t"         \
		".align 4\n\t"                  \
		"3:\n\t"                        \
		"popq %%rbp\n\t"                \
		: 
		: "a" (cd), "b" (nd),
		  "S" (tok), "D" (timeout),
		  "c" (&(scb->curr_thd)), "d" (next)
		: "memory", "cc", "r8", "r9", "r11", "r12", "r13", "r14", "r15");
#else
	__asm__ __volatile__ (              \
		"pushl %%ebp\n\t"               \
		"movl %%esp, %%ebp\n\t"         \
		"movl $2f, (%%eax)\n\t"         \
		"movl %%esp, 4(%%eax)\n\t"      \
		"cmp $0, 4(%%ebx)\n\t"          \
		"je 1f\n\t"                     \
		"movl %%edx, (%%ecx)\n\t"       \
		"movl 4(%%ebx), %%esp\n\t"      \
		"jmp *(%%ebx)\n\t"              \
		".align 4\n\t"                  \
		"1:\n\t"                        \
		"movl $3f, %%ecx\n\t"           \
		"movl %%edx, %%eax\n\t"         \
		"inc %%eax\n\t"                 \
		"shl $16, %%eax\n\t"            \
		"movl $0, %%ebx\n\t"            \
		"movl $0, %%esi\n\t"            \
		"movl %%edi, %%edx\n\t"         \
		"movl $0, %%edi\n\t"            \
		"sysenter\n\t"                  \
		"jmp 3f\n\t"                    \
		".align 4\n\t"                  \
		"2:\n\t"                        \
		"movl $0, 4(%%ebx)\n\t"         \
		".align 4\n\t"                  \
		"3:\n\t"                        \
		"popl %%ebp\n\t"                \
		: "=S" (htok)
		: "a" (cd), "b" (nd),
		  "S" (tok), "D" (timeout),
		  "c" (&(scb->curr_thd)), "d" (next)
		: "memory", "cc");
#endif
	scb = slm_scb_info_core();
	//if (pre_tok != tok) return -EAGAIN;
	return 0;
}

static inline int
slm_thd_activate(struct slm_thd *curr, struct slm_thd *t, sched_tok_t tok, int inherit_prio)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;
	struct slm_global      *g   = slm_global();
	struct cos_dcb_info    *cd  = slm_thd_dcbinfo(curr), *nd = slm_thd_dcbinfo(t);
	tcap_prio_t             prio;
	tcap_time_t             timeout;
	int                     ret = 0;
	volatile struct cos_scb_info *scb = slm_scb_info_core();


	timeout = g->timeout_next;
	prio = inherit_prio ? curr->priority : t->priority;

	if (unlikely(t->properties & (SLM_THD_PROPERTY_SEND | SLM_THD_PROPERTY_OWN_TCAP | SLM_THD_PROPERTY_SPECIAL))) {
		if (t == &g->sched_thd) {
			timeout = TCAP_TIME_NIL;
			prio    = curr->priority;
		}
		if (t->properties & SLM_THD_PROPERTY_SEND) {
			ret = cos_sched_asnd(t->asnd, g->timeout_next, g->sched_thd.rcv, tok);
			return ret;
		} else if (t->properties & SLM_THD_PROPERTY_OWN_TCAP) {
			ret = cos_switch(t->thd, t->tc, prio, timeout, g->sched_thd.rcv, tok);
			return ret;
		}
	}
	if (!cd || !nd) {
		assert(scb->timer_pre == timeout);
		scb->timer_pre = timeout;
		ret = cos_defswitch(t->thd, prio, timeout, tok);
	} else {
		ret = cos_ulswitch(curr->thd, t->thd, cd, nd, prio, timeout, tok);
	}

	if (unlikely(ret == -EPERM && !slm_thd_normal(t))) {
		/*
		 * Attempting to activate scheduler thread or idle
		 * thread failed for no budget in it's tcap. Force
		 * switch to the scheduler with current tcap.
		 */
		ret = cos_switch(g->sched_thd.thd, 0, curr->priority, TCAP_TIME_NIL, g->sched_thd.rcv, tok);
	}

	return ret;
}

static inline void slm_cs_exit(struct slm_thd *switchto, slm_cs_flags_t flags);
static inline int slm_cs_enter(struct slm_thd *current, slm_cs_flags_t flags);
/*
 * Do a few things: 1. call schedule to find the next thread to run,
 * 2. release the critical section (note this will cause visual
 * asymmetries in your code if you call slm_cs_enter before this
 * function), and 3. switch to the given thread. It hides some races,
 * and details that would make this difficult to write repetitively.
 *
 * Preconditions: if synchronization is required with code before
 * calling this, you must call slm_cs_enter before-hand (this is likely
 * a typical case).
 *
 * Return: the return value from cos_switch.  The caller must handle
 * this value correctly.
 *
 * A common use-case is:
 *
 * slm_cs_enter(...);
 * scheduling_stuff()
 * slm_cs_exit_reschedule(...);
 *
 * ...which correctly handles any race-conditions on thread selection and
 * dispatch.
 */
static inline int
slm_cs_exit_reschedule(struct slm_thd *curr, slm_cs_flags_t flags)
{
	volatile struct cos_scb_info *scb = slm_scb_info_core();
	struct cos_compinfo    *ci  = &cos_defcompinfo_curr_get()->ci;
	struct slm_global      *g   = slm_global();
	struct slm_thd         *t;
	sched_tok_t             tok;
	int                     ret;
	s64_t    diff;

try_again:
	tok  = cos_sched_sync();
	if (flags & SLM_CS_CHECK_TIMEOUT && g->timer_set) {
		cycles_t now = slm_now();

		/* Do we need to recompute the timer? */
		if (!cycles_greater_than(g->timer_next, now)) {
			g->timer_set = 0;
			/* The timer policy will likely reset the timer */
			slm_timer_expire(now);
		}
	}

	/* Make a policy decision! */
	t = slm_sched_schedule();
	if (unlikely(!t)) t = &g->idle_thd;

	assert(slm_state_is_runnable(t->state));
	slm_cs_exit(NULL, flags);

	ret = slm_thd_activate(curr, t, tok, 0);
	/* Assuming only the single tcap with infinite budget...should not get EPERM */
	assert(ret != -EPERM);
	
	//if (unlikely(ret != 0)) {
		/* Assuming only the single tcap with infinite budget...should not get EPERM */
	//	assert(ret != -EPERM);

		/* If the slm_thd_activate returns -EAGAIN, this means this scheduling token is outdated, try again */
	//	slm_cs_enter(curr, SLM_CS_NONE);
	//	goto try_again;
	//}

	return ret;
}

#endif	/* SLM_PRIVATE_H */
