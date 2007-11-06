/**
 * Hijack, or Asymmetric Execution Domains support for Linux
 * By Gabriel Parmer, gabep1@cs.bu.edu
 *
 * GPL V2 License (FIXME: copy and paste license here)
 */

#ifndef COS_COMPONENT_H
#define COS_COMPONENT_H

#include "../../../include/consts.h"
#include "../../../include/cos_types.h"

extern struct cos_sched_data_area cos_sched_notifications;

#define cos_syscall_asm \
	__asm__ __volatile__(                        \
		"pushl %%ebp\n\t"                    \
		"movl %%esp, %%ebp\n\t"              \
		"movl $1f, %%ecx\n\t"                \
		"movl %1, %%eax\n\t"                 \
		"sysenter\n\t"                       \
		"1:\n\t"                             \
		"movl %%eax, %0\n\t"                 \
		"popl %%ebp"                         \
		: "=r" (ret)
#define cos_syscall_clobber \
                : "%eax", "%ecx", "%edx", "cc");     \
                                                     \
	return ret;                                  \

#define cos_syscall_0(num, rtype, name)              \
static inline rtype cos_##name(void)                 \
{                                                    \
	rtype ret;                                   \
cos_syscall_asm                                      \
		: "r" (num<<16)                      \
cos_syscall_clobber                                  \
}

#define cos_syscall_1(num, rtype, name, type0, name0)\
static inline rtype cos_##name(type0 name0)          \
{                                                    \
	rtype ret;                                   \
cos_syscall_asm                                      \
		: "r" (num<<16), "b" (name0)         \
cos_syscall_clobber                                  \
}

#define cos_syscall_2(num, rtype, name, type0, name0, type1, name1)\
static inline rtype cos_##name(type0 name0, type1 name1) \
{                                                    \
	rtype ret;                                   \
cos_syscall_asm                                      \
		: "r" (num<<16), "b" (name0), "S" (name1) \
cos_syscall_clobber                                  \
}

#define cos_syscall_3(num, rtype, name, type0, name0, type1, name1, type2, name2) \
static inline rtype cos_##name(type0 name0, type1 name1, type2 name2) \
{                                                    \
	rtype ret;                                   \
cos_syscall_asm                                      \
		: "g" (num<<16), "b" (name0), "S" (name1), "D" (name2) \
cos_syscall_clobber                                  \
}


#define cos_syscall_4(num, rtype, name, type0, name0, type1, name1, type2, name2, type3, name3) \
static inline rtype cos_##name(type0 name0, type1 name1, type2 name2, type3 name3) \
{                                                    \
	rtype ret;                                   \
cos_syscall_asm                                      \
		: "g" (num<<16), "b" (name0), "S" (name1), "D" (name2), "d" (name3) \
                : "%eax", "%ecx", "cc");             \
                                                     \
	return ret;                                  \
}

typedef __attribute__((regparm(1))) void (*create_thd_fn_t)(void *data);

cos_syscall_0(1, int, resume_return);
cos_syscall_0(2, int, get_thd_id);
cos_syscall_4(3, int, create_thread, create_thd_fn_t, fn, vaddr_t, stack, void*, data, int, urgency);
cos_syscall_0(4, int, __switch_thread);
cos_syscall_2(5, int, kill_thd, int, kill_thdid, int, switchto_thdid);
cos_syscall_0(6, int, brand_upcall);
cos_syscall_2(7, int, brand, int, thd_id, int, flags);
cos_syscall_1(8, int, upcall, int, spd_id);
cos_syscall_3(9, int, sched_cntl, int, operation, int, thd_id, long, option);

/*
 * We cannot just pass the thread id into the system call in registers
 * as the current thread of control making the switch_thread system
 * call might be preempted after deciding based on memory structures
 * which thread to run, but before the actual system call is made.
 * The preempting thread might change the current threads with high
 * priority.  When the system call ends up being executed, it is on
 * stale info, and a thread is switched to that might be actually be
 * interesting.
 *
 * Storing in memory the intended thread to switch to, allows other
 * preempting threads to update the next_thread even if a thread is
 * preempted between logic and calling switch_thread.
 */
static inline int cos_switch_thread(unsigned short int thd_id, unsigned short int flags, unsigned int urgency)
{
        /* This must be volatile as we must commit what we want to
	 * write to memory immediately to be read by the kernel */
	volatile struct cos_sched_next_thd *cos_next = &cos_sched_notifications.cos_next;

	cos_next->next_thd_id = thd_id;
	cos_next->next_thd_flags = flags;
	cos_next->next_thd_urgency = urgency;

	/* kernel will read next thread information from cos_next */
	return cos___switch_thread(); 
}

/* from linux source in string.h */
static inline void * cos_memcpy(void * to, const void * from, int n)
{
	int d0, d1, d2;
	
	__asm__ __volatile__(
        "rep ; movsl\n\t"
        "movl %4,%%ecx\n\t"
        "andl $3,%%ecx\n\t"
#if 1   /* want to pay 2 byte penalty for a chance to skip microcoded rep? */
        "jz 1f\n\t"
#endif
        "rep ; movsb\n\t"
        "1:"
        : "=&c" (d0), "=&D" (d1), "=&S" (d2)
        : "0" (n/4), "g" (n), "1" ((long) to), "2" ((long) from)
        : "memory");
	
	return (to);
	
}

#define prevent_tail_call(ret) __asm__ ("" : "=r" (ret) : "m" (ret))
#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

#define COS_FIRST_ARG ((void *)SHARED_REGION_START)

#endif
