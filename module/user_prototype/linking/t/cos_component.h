/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef COS_COMPONENT_H
#define COS_COMPONENT_H

#include "../../../include/consts.h"
#include "../../../include/cos_types.h"

extern struct cos_sched_data_area cos_sched_notifications;
extern volatile long cos_this_spd_id;
extern void *cos_heap_ptr;

/*
 * A note on the arguments to and for all system calls and on the
 * planned evolution of the system call layer:
 *
 * System calls started as they exist in any operating system, simply
 * passing some identifier to signify which system call we are
 * invoking (in eax), and passing arguments in the rest of the
 * registers.  Because we are sharing a system call namespace
 * (essentially) with Linux using Hijacking techiques, we pass
 * syscall<<16 to signify our system calls.
 * 
 * The second evolution required that we are able to identify which
 * spd makes an system call which is not self-evident (as it would be
 * in a normal system) when composite spds are taken into account.
 * When invoking capabilities for inter-component communication, this
 * information is explicit in the capability itself.  So we needed
 * this for system calls as well.  Version two is that we pass in edx
 * the spd id whenever we make a system call.  Of course component
 * writers themselves cannot be bothered with this, so we store the
 * spd_id in each component in memory and simply read this address,
 * passing its value on system calls.  This implies that the loader
 * initializes this memory location with the correct spd id when the
 * spd is loaded into memory, which is not altogether pleasant.
 *
 * The third evolution which has yet to occur is that system calls
 * will be pulled into the capability framework.  They will not invoke
 * the general path for ipc as the extra check would slow it down, but
 * will invoke capabilities within a separate region of the namespace,
 * allowing for the early demuxing in kern_entry.S.  This is an
 * important evolution because 1) it solves the problem we already
 * have with identifying which spd is making a system call in a more
 * natural (allbeit slower) manner, and 2) it allows services to be
 * migrated into and out of the kernel itself dynamically, given
 * perceived system call overhead and application progress.
 */


#define cos_syscall_asm \
	__asm__ __volatile__(                        \
		"pushl %%ebp\n\t"                    \
		"movl %%esp, %%ebp\n\t"              \
		"movl $1f, %%ecx\n\t"                \
		"sysenter\n\t"                       \
		"1:\n\t"                             \
		"movl %%eax, %0\n\t"                 \
		"popl %%ebp"                         \
		: "=r" (ret)
#define cos_syscall_clobber \
                : "%ecx", "cc");     \
                                                     \
	return ret;                                  \

#define cos_syscall_0(num, rtype, name)              \
static inline rtype cos_##name(void)                 \
{                                                    \
	rtype ret;                                   \
cos_syscall_asm                                      \
		: "a" (num<<16), "d" (cos_this_spd_id) \
cos_syscall_clobber                                  \
}

#define cos_syscall_1(num, rtype, name, type0, name0)\
static inline rtype cos_##name(type0 name0)          \
{                                                    \
	rtype ret;                                   \
cos_syscall_asm                                      \
		: "a" (num<<16), "d" (cos_this_spd_id), "b" (name0) \
cos_syscall_clobber                                  \
}

#define cos_syscall_2(num, rtype, name, type0, name0, type1, name1)\
static inline rtype cos_##name(type0 name0, type1 name1) \
{                                                    \
	rtype ret;                                   \
cos_syscall_asm                                      \
		: "a" (num<<16), "d" (cos_this_spd_id), "b" (name0), "S" (name1) \
cos_syscall_clobber                                  \
}

#define cos_syscall_3(num, rtype, name, type0, name0, type1, name1, type2, name2) \
static inline rtype cos_##name(type0 name0, type1 name1, type2 name2) \
{                                                    \
	rtype ret;                                   \
cos_syscall_asm                                      \
		: "a" (num<<16), "d" (cos_this_spd_id), "b" (name0), "S" (name1), "D" (name2) \
cos_syscall_clobber                                  \
}

typedef __attribute__((regparm(1))) void (*create_thd_fn_t)(void *data);

cos_syscall_0(1, int, resume_return);
//cos_syscall_0(2, int, get_thd_id);
cos_syscall_3(3, int, create_thread, create_thd_fn_t, fn, vaddr_t, stack, void*, data);
cos_syscall_0(4, int, __switch_thread);
cos_syscall_2(5, int, kill_thd, int, kill_thdid, int, switchto_thdid);
cos_syscall_3(6, int, __brand_upcall, int, thd_id_flags, long, arg1, long, arg2);
cos_syscall_2(7, int, brand_cntl, int, thd_id, int, flags);
cos_syscall_1(8, int, upcall, int, spd_id);
cos_syscall_3(9, int, sched_cntl, int, operation, int, thd_id, long, option);
cos_syscall_1(10, int, mpd_cntl, int, operation);
cos_syscall_3(11, int, __mmap_cntl, long, op_flags_dspd, long, daddr, long, mem_id);

static inline int cos_mmap_cntl(short int op, short int flags, 
				short int dest_spd, vaddr_t dest_addr, long mem_id) {
	/* encode into 3 arguments */
	return cos___mmap_cntl(((op<<24) | (flags << 16) | (dest_spd)), 
			       dest_addr, mem_id);
}

static inline int cos_brand_upcall(short int thd_id, short int flags, long arg1, long arg2)
{
	return cos___brand_upcall((thd_id << 16) | flags, arg1, arg2);
}

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
static inline int cos_switch_thread(unsigned short int thd_id, unsigned short int flags, 
				    unsigned int urgency)
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

static inline int cos_get_thd_id(void)
{
	struct shared_user_data *ud = (void *)COS_INFO_REGION_ADDR;

	return ud->current_thread;
}

static inline void *cos_get_arg_region(void)
{
	struct shared_user_data *ud = (void *)COS_INFO_REGION_ADDR;

	return ud->argument_region;
}

static inline long cos_spd_id(void)
{
	return cos_this_spd_id;
}

static inline void *cos_get_heap_ptr(void)
{
	return cos_heap_ptr;
}

static inline void cos_set_heap_ptr(void *addr)
{
	cos_heap_ptr = addr;
}

static inline long cos_cmpxchg(void *memory, long anticipated, long result)
{
	long ret;

	__asm__ __volatile__(
		"call cos_atomic_cmpxchg"
		: "=r" (ret)
		: "a" (anticipated), "b" (memory), "c" (result)
		: "cc");

	return ret;
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
