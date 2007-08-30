#include "../../../include/consts.h"
#include "../../../include/cos_types.h"

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
                : "%eax", "%ecx", "%edx");           \
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
		: "r" (num<<16), "b" (name0), "S" (name1), "D" (name2) \
cos_syscall_clobber                                  \
}

typedef void (*create_thd_fn_t)(void *data);

cos_syscall_1(1, int, resume_return, int, thd_id);
cos_syscall_0(2, int, get_thd_id);
cos_syscall_0(3, int, create_thread);//, create_thd_fn_t, fn, vaddr_t, stack, void*, data);
cos_syscall_1(4, int, switch_thread, int, thd_id);
cos_syscall_2(5, int, kill_thd, int, kill_thdid, int, switchto_thdid);

/* 
 * This is wrong and must be done in assembly to avoid stack pushes
 * before we have a stack.
 */
__attribute__((regparm(3)))
void cos__start_thd(create_thd_fn_t fn, vaddr_t stack, void *data)
{
	__asm__ __volatile__(
		"movl %0, %%esp\n\t"
		"subl $40, %%esp\n\t"
		"pushl %1\n\t"
		"call *(%2)\n\t"
		:: "r" (stack), "r" (data), "r" (fn)
		);

	cos_kill_thd(-1, 0); /* kill ourselves */
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

#define COS_FIRST_ARG ((void *)SHARED_REGION_START)

