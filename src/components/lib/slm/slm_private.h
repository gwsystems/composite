#ifndef SLM_PRIVATE_H
#define SLM_PRIVATE_H

#include <cos_component.h>
#include <ps.h>
#include <slm_api.h>

#ifndef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif

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

static inline u32_t
slm_rdpkru(void)
{
	u32_t pkru;
	asm volatile(
		"xor %%rcx, %%rcx\n\t"
		"xor %%rdx, %%rdx\n\t"
		"rdpkru"
		: "=a" (pkru)
		:
		: "rcx", "rdx"
	);

	return pkru;
}

/*
 * If the current thread is the scheduler or idle thread, return that
 * slm_thd. That thread should generally never be used
 */
struct slm_thd *slm_thd_special(void);

static inline int
cos_ulswitch(struct slm_thd *curr, struct slm_thd *next, struct cos_dcb_info *cd, struct cos_dcb_info *nd, tcap_prio_t prio, tcap_time_t timeout, sched_tok_t tok)
{
	struct cos_defcompinfo       *defci     = cos_defcompinfo_curr_get();
	struct cos_compinfo          *ci        = cos_compinfo_get(defci);
	volatile struct cos_scb_info *scb       = (struct cos_scb_info*)ci->scb_uaddr + cos_cpuid();
	
	unsigned long pre_tok = tok;

	if (curr == next) {
		return 0; 
	}
	assert(scb);
	/* 
	 * If previous timer smaller than current timeout, we need to take 
	 * the kernel path to update the timer.
	 */

	if (scb->timer_pre < timeout) {
		scb->timer_pre = timeout;
		return cos_defswitch(next->thd, prio, timeout, tok);
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

#ifndef __x86_64__
	printc("User-level thread dispatch is not supported in 32bit.\n");
	assert(0);
	/*__asm__ __volatile__ (              \
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
		: "=S" (pre_tok)
		: "a" (cd), "b" (nd),
		  "S" (tok), "D" (timeout),
		  "c" (&(scb->curr_thd)), "d" (next->thd)
		: "memory", "cc");*/
#endif

#if defined(__PROTECTED_DISPATCH__)
	__asm__ __volatile__ (
		"pushq %%rbp\n\t"                                         \
		"mov %%rsp, %%rbp\n\t"                                    \
		/* Save rax since it will be cleared by wrpkru */
		"mov %%rax, %%r11\n\t"                                    \
		"movabs $0xdeadbeefdeadbeef, %%r15\n\t"                   \
		/* Size of cos_dcb_info struct. */
		"movabs $" STR(COS_DCB_INFO_SIZE) ", %%r14\n\t"           \
		/* Offset of current thread. */
		"imulq %%r14, %%rdx\n\t"                                  \
		/* Save thread id of the next thread for possible scb updates. */
		"mov %%rsi, %%r12\n\t"                                    \
		/* Offset of next thread. */
		"imulq %%r14, %%rsi\n\t"                                  \
		/* Hardcoded DCB base address. */
		"movabs $" STR(ULK_DCB_ADDR) ", %%r14\n\t"                \
		/* dcb_addr of current thread. */
		"addq %%r14, %%rdx\n\t"                                   \
		/* dcb_addr of next thread. */
		"addq %%r14, %%rsi\n\t"                                   \
		/* Save content in %rcx and %rdx, which will be cleared for wrpkru. */
		"movq %%rcx, %%r8\n\t"                                    \
		"movq %%rdx, %%r9\n\t"                                    \
		/* Hardcoded protection domain of ulk. */
		"movl $" STR(MPK_KEY2REG(ULK_MPK_KEY)) ",%%eax\n\t"       \
		"xor %%rcx, %%rcx\n\t"                                    \
		"xor %%rdx, %%rdx\n\t"                                    \
		/* Switch protection domain. */
		"wrpkru\n\t"                                              \
		"movabs $0xdeadbeefdeadbeef, %%r10\n\t"                   \
		"cmp %%r10, %%r15\n\t"                                    \
		/* TODO: error handling */
		/* Update ip and sp in the dcb of the current thread. */
		"movabs $2f, %%r10\n\t"                                   \
		"mov %%r10, " STR(COS_DCB_IP_OFFSET) "(%%r9)\n\t"         \
		"mov %%rsp, " STR(COS_DCB_SP_OFFSET) "(%%r9)\n\t"         \
		/* Read out the sp from the dcb of next thread */
		"mov 8(%%rsi), %%r14\n\t"                                 \
		/* Check if sp in dcb of the next thread equals zero. */
		"cmp $0, %%r14\n\t"                                       \
		/* If it equals 0, Take kernel path. */
		"je 1f\n\t"                                               \
		/*
		 * Read out the ip from the dcb of next thread.
		 * Can't jmp directly, need to switch back to scheduler's protection domain first.
		 */
		"mov (%%rsi), %%r13\n\t"                                  \
		/* Update current active thread in the scb. */
		/* Get core ID */
		/* "rdpid %%rdx\n\t"                                      \ */
		/* "movq %%rdx, %%rax\n\t"                                \ */
		"rdtscp\n\t"                                              \
		"movq %%rcx, %%rax\n\t"                                   \
		"andq $0xFFF, %%rax\n\t"                                  \
		/* Get per-core scb info */
		/* Hardcoded size of scb_info. */
		"movabs $" STR(COS_SCB_INFO_SIZE) ", %%rcx\n\t"           \
		"imulq %%rcx, %%rax\n\t"                                  \
		/* Base address of scb. */
		"movabs $" STR(ULK_SCB_ADDR) ", %%rcx\n\t"                \
		"addq %%rcx, %%rax\n\t"                                   \
		/* Update tid in scb */
		"shl $16 , %%r11\n\t"                                     \
		"movq %%r11, %%rcx\n\t"                                   \
		"or %%rcx, %%r12\n\t"                                     \
		"movq %%r12, (%%rax)\n\t"                                 \
		/*"movq %%r12, " STR(COS_SCB_TID_OFFSET) "(%%rax)\n\t"      \*/
		/* Update thdcap in scb */
		/*"movq %%r11, " STR(COS_SCB_THDCAP_OFFSET) "(%%rax)\n\t"   \*/
		/* Return to scheduler's protection domain. */
		"movl $" STR(MPK_KEY2REG(SCHED_MPK_KEY)) ",%%eax\n\t"     \
		"xor %%rcx, %%rcx\n\t"                                    \
		"xor %%rdx, %%rdx\n\t"                                    \
		"wrpkru\n\t"                                              \
		"movabs $0xdeadbeefdeadbeef, %%r10\n\t"                   \
		"cmp %%r10, %%r15\n\t"                                    \
		/* TODO: error handling */
		/* restore sp to what we just read out in r14. */
		"mov %%r14, %%rsp\n\t"                                    \
		/* jmp to the ip which we just read out in r13. */
		"jmp *%%r13\n\t"                                          \
		/* Kernel Path */
		/*
		 * Haven't switch back to scheduler's protection domain yet.
		 * But, we don't have to do it immediately. We can do it at 3f,
		 * since we have to have the return logic at 3f to prevent the case
		 * which we got an interrupt before switch protection domain back
		 * to the scheduler.
		 */
		".align 8\n\t"                                            \
		"1:\n\t"                                                  \
		"movl $" STR(MPK_KEY2REG(SCHED_MPK_KEY)) ",%%eax\n\t"     \
		"xor %%rcx, %%rcx\n\t"                                    \
		"xor %%rdx, %%rdx\n\t"                                    \
		"wrpkru\n\t"                                              \
		"movabs $0xdeadbeefdeadbeef, %%r10\n\t"                   \
		"cmp %%r10, %%r15\n\t"                                    \
		/* TODO: error handling */
		/* Kernel path starts from here. */
		"movabs $3f, %%r8\n\t"                                    \
		/* Mov thdcap of next thread to rax. */
		"mov %%r11, %%rax\n\t"                                    \
		"inc %%rax\n\t"                                           \
		"shl $16, %%rax\n\t"                                      \
		"mov $0, %%rsi\n\t"                                       \
		/* Mov timeout into rdx */
		"mov %%rdi, %%rdx\n\t"                                    \
		"mov $0, %%rdi\n\t"                                       \
		"syscall\n\t"                                             \
		"jmp 3f\n\t"                                              \
		/*
		 * User-level path. Already switched back from thd_next,
		 * now clear sp in the dcb of the current thread. 
		 */
		".align 8\n\t"                                            \
		"2:\n\t"                                                  \
		"movabs $0x1234123412341234, %%r15\n\t"                   \
		/* Hardcode protection domain of ulk. */
		"movl $" STR(MPK_KEY2REG(ULK_MPK_KEY)) ",%%eax\n\t"       \
		"xor %%rcx, %%rcx\n\t"                                    \
		"xor %%rdx, %%rdx\n\t"                                    \
		/* Switch protection domain. */
		"wrpkru\n\t"                                              \
		"movabs $0x1234123412341234, %%r10\n\t"                   \
		"cmp %%r15, %%r10\n\t"                                    \
		/* TODO: error handling */
		/* clear sp in dcb of next thread. */
		"movq $0, 8(%%rsi)\n\t"                                   \
		/* Hardcode protection domain of scheduler. */
		"movl $" STR(MPK_KEY2REG(SCHED_MPK_KEY)) ",%%eax\n\t"     \
		"xor %%rcx, %%rcx\n\t"                                    \
		"xor %%rdx, %%rdx\n\t"                                    \
		/* Switch protection domain. */
		"wrpkru\n\t"                                              \
		"movabs $0x123456789abcdef0, %%r10\n\t"                   \
		"cmp %%r10, %%r15\n\t"                                    \
		/* TODO: error handling */
		".align 8\n\t"                                            \
		"3:\n\t"                                                  \
		"xor %%rcx, %%rcx\n\t"                                    \
		"xor %%rdx, %%rdx\n\t"                                    \
		"rdpkru\n\t"                                              \
		"popq %%rbp\n\t"                                          \
		:
		: "a" (next->thd), "S" (next->tid),
		  "b" (tok), "D" (timeout),
		  "d" (curr->tid)
		: "memory", "cc", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15");

#else
	u64_t thdpack = (next->thd << 16) | next->tid;
	__asm__ __volatile__ (
		"pushq %%rbp\n\t"               \
		"mov %%rsp, %%rbp\n\t"          \
		"movabs $2f, %%r8\n\t"          \
		"mov %%r8, (%%rax)\n\t"         \
		"mov %%rsp, 8(%%rax)\n\t"       \
		"cmp $0, 8(%%rsi)\n\t"          \
		"je 1f\n\t"                     \
		/* "mov %%rdx, %%rax\n\t"          \ */
		/* "and $0xFFFF, %%rax\n\t"        \ */
		/* "mov %%rax, -8(%%rcx)\n\t"      \ */
		/* "shr $16, %%rdx\n\t"            \ */
		/* "mov %%rdx, (%%rcx)\n\t"        \ */
		"mov %%rdx, (%%rcx)\n\t"        \
		"mov 8(%%rsi), %%rsp\n\t"       \
		"jmp *(%%rsi)\n\t"              \
		".align 8\n\t"                  \
		"1:\n\t"                        \
		"movabs $3f, %%r8\n\t"          \
		"shr $16, %%rdx\n\t"            \
		"mov %%rdx, %%rax\n\t"          \
		"inc %%rax\n\t"                 \
		"shl $16, %%rax\n\t"            \
		"mov $0, %%rsi\n\t"             \
		"mov %%rdi, %%rdx\n\t"          \
		"mov $0, %%rdi\n\t"             \
		"syscall\n\t"                   \
		"jmp 3f\n\t"                    \
		".align 8\n\t"                  \
		"2:\n\t"                        \
		"movq $0, 8(%%rsi)\n\t"         \
		".align 8\n\t"                  \
		"3:\n\t"                        \
		"popq %%rbp\n\t"                \
		:
		: "a" (cd), "S" (nd),
		  "b" (tok), "D" (timeout),
		  "c" (&(scb->thdpack)), "d" (thdpack)
		: "memory", "cc", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15");
#endif
	scb = slm_scb_info_core();
	assert(scb);

	if (pre_tok != scb->sched_tok) {
		return -EAGAIN;
	}
	return 0;
}

static inline int
slm_thd_activate(struct slm_thd *curr, struct slm_thd *t, sched_tok_t tok, int inherit_prio)
{
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
#if defined (__SLITE__)
	if (!cd || !nd || (curr->vasid != t->vasid)) {
		if (scb->timer_pre < timeout) {
			scb->timer_pre = timeout;
		}
		ret = cos_defswitch(t->thd, prio, timeout, tok);
	} else {
		ret = cos_ulswitch(curr, t, cd, nd, prio, timeout, tok);
	}
#else
	ret = cos_defswitch(t->thd, prio, timeout, tok);
#endif

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
	struct cos_compinfo    *ci  = &cos_defcompinfo_curr_get()->ci;
	struct slm_global      *g   = slm_global();
	struct slm_thd         *t;
	sched_tok_t             tok;
	int                     ret;

try_again:
	tok  = cos_sched_sync(ci);
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
	if (unlikely(ret != 0)) {
		/* Assuming only the single tcap with infinite budget...should not get EPERM */
		assert(ret != -EPERM);
		assert(ret != -EINVAL);

		/*
		 * If the slm_thd_activate returns -EBUSY, this means we are trying to switch to the scheduler thread,
		 * and scheduler thread still has pending events. Directly return to process pending events.
		 */
		if (ret == -EBUSY) return ret;
		/* If the slm_thd_activate returns -EAGAIN, this means this scheduling token is outdated, try again */
		if (ret == -EBUSY) return ret;
		assert(ret == -EAGAIN);
		slm_cs_enter(curr, SLM_CS_NONE);
		goto try_again;
	}

	return ret;
}

#endif	/* SLM_PRIVATE_H */
