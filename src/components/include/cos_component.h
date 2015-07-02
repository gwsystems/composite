/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef COS_COMPONENT_H
#define COS_COMPONENT_H

#include <consts.h>
#include <cos_types.h>
#include <errno.h>
#include <util.h>

/* temporary */
static inline 
int call_cap_asm(u32_t cap_no, u32_t op, int arg1, int arg2, int arg3, int arg4)
{
        long fault = 0;
	int ret;

	cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;
	cap_no += op;

//	__asm__ __volatile__("":::"ecx", "edx");
	__asm__ __volatile__( \
		"pushl %%ebp\n\t" \
		"movl %%esp, %%ebp\n\t" \
		"movl $1f, %%ecx\n\t" \
		"sysenter\n\t" \
		".align 8\n\t" \
		"jmp 2f\n\t" \
		".align 8\n\t" \
		"1:\n\t" \
		"popl %%ebp\n\t" \
		"movl $0, %%ecx\n\t" \
		"jmp 3f\n\t" \
		"2:\n\t" \
		"popl %%ebp\n\t" \
		"movl $1, %%ecx\n\t" \
		"3:" \
		: "=a" (ret), "=c" (fault)
		: "a" (cap_no), "b" (arg1), "S" (arg2), "D" (arg3), "d" (arg4) \
		: "memory", "cc");

	__asm__ __volatile__("":::"eax", "ebx", "ecx", "edx", "esi", "edi");

	return ret;
}

static inline int 
cap_switch_thd(u32_t cap_no) 
{
	return call_cap_asm(cap_no, 0, 0, 0, 0, 0);
}

static inline int 
call_cap(u32_t cap_no, int arg1, int arg2, int arg3, int arg4)
{
	return call_cap_asm(cap_no, 0, arg1, arg2, arg3, arg4);
}

static inline int 
call_cap_op(u32_t cap_no, u32_t op_code,int arg1, int arg2, int arg3, int arg4)
{
	return call_cap_asm(cap_no, op_code, arg1, arg2, arg3, arg4);
}

static void 
cos_print(char *s, int len)
{ call_cap(PRINT_CAP_TEMP, (int)s, len, 0, 0); }

/**
 * FIXME: Please remove this since it is no longer needed
 */

extern long stkmgr_stack_space[ALL_TMP_STACKS_SZ];

extern struct cos_component_information cos_comp_info;

/*
 * A note on the arguments to and for all system calls and on the
 * planned evolution of the system call layer:
 *
 * System calls started as they exist in any operating system, simply
 * passing some identifier to signify which system call we are
 * invoking (in eax), and passing arguments in the rest of the
 * registers.  Because we are sharing a system call namespace
 * (essentially) with Linux using Hijacking techiques, we pass
 * syscall<<COS_SYSCALL_OFFSET to signify our system calls.
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
 * natural (albeit slower) manner, and 2) it allows services to be
 * migrated into and out of the kernel itself dynamically, given
 * perceived system call overhead and application progress.
 */

/* 
 * The ABI for syscalls regarding registers is that any registers you
 * want saved, must be saved by you.  This is why the extensive
 * clobber list is used in the inline assembly for making the syscall.
 */

/* 
 * The extra asm below is rediculous as gcc doesn't let us clobber
 * registers already in the input/output positions, but we DO clobber
 * them in this operation.  I can't clobber ebp in the clobber list,
 * so I do it manually.  This is right up there with hideous.
 */

#define cos_syscall_asm \
	__asm__ __volatile__("":::"eax", "ecx", "edx", "esi", "edi");	\
	__asm__ __volatile__(                        \
	        "pushl %%ebx\n\t"                    \
	        "pushl %%ecx\n\t"                    \
	        "pushl %%edx\n\t"                    \
	        "pushl %%esi\n\t"                    \
	        "pushl %%edi\n\t"                    \
	        "pushl %%ebp\n\t"                    \
		"movl %%esp, %%ebp\n\t"              \
		"movl $1f, %%ecx\n\t"                \
		"sysenter\n\t"                       \
		"1:\n\t"                             \
		"movl %%eax, %0\n\t"                 \
		"popl %%ebp\n\t"                     \
		"popl %%edi\n\t"                     \
		"popl %%esi\n\t"                     \
		"popl %%edx\n\t"                     \
		"popl %%ecx\n\t"                     \
		"popl %%ebx"                         \
		: "=a" (ret)
#define cos_syscall_clobber			     \
	: "memory", "cc");			     \
	return ret;

#define cos_syscall_0(num, rtype, name)              \
static inline rtype cos_##name(void)                 \
{                                                    \
	rtype ret;                                   \
cos_syscall_asm                                      \
		: "a" (num<<COS_SYSCALL_OFFSET), "d" (cos_comp_info.cos_this_spd_id) \
cos_syscall_clobber                                  \
}

#define cos_syscall_1(num, rtype, name, type0, name0)\
static inline rtype cos_##name(type0 name0)          \
{                                                    \
	rtype ret;                                   \
cos_syscall_asm                                      \
		: "a" (num<<COS_SYSCALL_OFFSET), "d" (cos_comp_info.cos_this_spd_id), "b" (name0) \
cos_syscall_clobber                                  \
}

#define cos_syscall_2(num, rtype, name, type0, name0, type1, name1)\
static inline rtype cos_##name(type0 name0, type1 name1) \
{                                                    \
	rtype ret;                                   \
cos_syscall_asm                                      \
		: "a" (num<<COS_SYSCALL_OFFSET), "d" (cos_comp_info.cos_this_spd_id), "b" (name0), "S" (name1) \
cos_syscall_clobber                                  \
}

#define cos_syscall_3(num, rtype, name, type0, name0, type1, name1, type2, name2) \
static inline rtype cos_##name(type0 name0, type1 name1, type2 name2) \
{                                                    \
	rtype ret;                                   \
cos_syscall_asm                                      \
		: "a" (num<<COS_SYSCALL_OFFSET), "d" (cos_comp_info.cos_this_spd_id), "b" (name0), "S" (name1), "D" (name2) \
cos_syscall_clobber                                  \
}

#define cos_syscall_4(num, rtype, name, type0, name0, type1, name1, type2, name2, type3, name3) \
static inline rtype cos_##name(type0 name0, type1 name1, type2 name2, type3 name3) \
{                                                    \
	rtype ret;                                   \
cos_syscall_asm                                      \
		: "a" (num<<COS_SYSCALL_OFFSET), "b" (name0), "S" (name1), "D" (name2), "d" (name3), \
cos_syscall_clobber                                  \
}

cos_syscall_0(1,  int, stats);
//cos_syscall_2(2,  int, print, char*, str, int, len);
cos_syscall_3(3,  int, create_thread, int, dest_spd_id, int, a, int, b);
cos_syscall_2(4,  int, __switch_thread, int, thd_id, int, flags);
cos_syscall_3(5, int, __async_cap_cntl, int, operation, int, arg1, long, arg2);
cos_syscall_1(6, int, areceive, int, acap_id);
cos_syscall_1(7, int, asend, int, acap_id);
cos_syscall_2(8,  int, upcall, int, spd_id, int, init_data);
cos_syscall_3(9,  int, sched_cntl, int, operation, int, thd_id, long, option);
cos_syscall_3(10, int, mpd_cntl, int, operation, spdid_t, composite_spd, spdid_t, composite_dest);
cos_syscall_3(11, int, __mmap_cntl, long, op_flags_dspd, vaddr_t, daddr, unsigned long, mem_id);
cos_syscall_3(12, int, acap_wire, long, thd_id, long, option, long, data);
cos_syscall_3(13, long, __cap_cntl, int, option, u32_t, arg1, long, arg2);
cos_syscall_3(14, int, __buff_mgmt, void *, addr, int, thd_id, int, len_option);
cos_syscall_3(15, int, __thd_cntl, int, op_thdid, long, arg1, long, arg2);
cos_syscall_0(16, int, idle);
cos_syscall_3(17, int, __spd_cntl, int, op_spdid, long, arg1, long, arg2);
cos_syscall_3(18, int, __vas_cntl, int, op_spdid, long, arg1, long, arg2);
cos_syscall_3(19, int, __trans_cntl, unsigned long, op_ch, unsigned long, addr, int, off);
cos_syscall_3(20, int, __pfn_cntl, unsigned long, op_spd, unsigned long, mem_id, int, extent);
cos_syscall_0(31,  int, null);

static inline int cos_mmap_cntl(short int op, short int flags, short int dest_spd, 
				vaddr_t dest_addr, unsigned long mem_id) {
	/* encode into 3 arguments */
	return cos___mmap_cntl(((op<<24) | (flags << 16) | (dest_spd)), 
			       dest_addr, mem_id);
}

static inline int cos_async_cap_cntl(int operation, unsigned short int arg1, unsigned short int arg2, int arg3)
{
	return cos___async_cap_cntl(operation, ((arg1 << 16) | (arg2 & 0xFFFF)), arg3);
}

/* 
 * Physical frame number manipulations.  Which component, and what
 * extent of physical frames are we manipulating. 
 */
static inline int 
cos_pfn_cntl(short int op, int dest_spd, unsigned int mem_id, int extent) {
	/* encode into 3 arguments */
	return cos___pfn_cntl(((op<<16) | (dest_spd)), mem_id, extent);
}

static inline int cos_buff_mgmt(unsigned short int op, void *addr, unsigned short int len, short int thd_id)
{
	return cos___buff_mgmt(addr, thd_id, ((len << 16) | (op & 0xFFFF)));
}

static inline int cos_thd_cntl(short int op, short int thd_id, long arg1, long arg2)
{
	return cos___thd_cntl(((op << 16) | (thd_id & 0xFFFF)), arg1, arg2);
}

static inline int cos_spd_cntl(short int op, short int spd_id, long arg1, long arg2)
{
	return cos___spd_cntl(((op << 16) | (spd_id & 0xFFFF)), arg1, arg2);
}

static inline int cos_vas_cntl(short int op, short int spd_id, long addr, long sz)
{
	return cos___vas_cntl(((op << 16) | (spd_id & 0xFFFF)), addr, sz);
}

static inline long cos_cap_cntl_spds(spdid_t cspd, spdid_t sspd, long arg)
{
	return cos___cap_cntl(COS_CAP_GET_INVCNT, ((cspd << 16) | (sspd & 0xFFFF)), arg);
}

static inline long cos_cap_cntl(short int op, spdid_t cspd, u16_t capid, long arg)
{
	return cos___cap_cntl(op, (cspd << 16) | (capid & 0xFFFF), arg);
}

static inline int cos_trans_cntl(int op, int channel, unsigned long addr, int off)
{
	return cos___trans_cntl(((op << 16) | (channel & 0xFFFF)), addr, off);
}

static inline long get_stk_data(int offset)
{
	unsigned long curr_stk_pointer;

	asm ("movl %%esp, %0;" : "=r" (curr_stk_pointer));
	/* 
	 * We save the CPU_ID and thread id in the stack for fast
	 * access.  We want to find the struct cos_stk (see the stkmgr
	 * interface) so that we can then offset into it and get the
	 * cpu_id.  This struct is at the _top_ of the current stack,
	 * and cpu_id is at the top of the struct (it is a u32_t).
	 */
	return *(long *)((curr_stk_pointer & ~(COS_STACK_SZ - 1)) + 
			 COS_STACK_SZ - offset * sizeof(u32_t));
}

#define GET_CURR_CPU cos_cpuid()

static inline long cos_cpuid(void)
{
#if NUM_CPU == 1
	return 0;
#endif
	/* 
	 * see comments in the get_stk_data above.
	 */
	return get_stk_data(CPUID_OFFSET);
}

static inline unsigned short int cos_get_thd_id(void)
{
	/* 
	 * see comments in the get_stk_data above.
	 */
	return get_stk_data(THDID_OFFSET);
}

#define ERR_THROW(errval, label) do { ret = errval; goto label; } while (0)

static inline void *cos_arg_region_base(void)
{
	struct shared_user_data *ud = (void *)COS_INFO_REGION_ADDR;

	return ud->argument_region;
}

static inline void *cos_get_arg_region(void)
{
	return cos_arg_region_base() + sizeof(struct pt_regs);
}

static inline void cos_mpd_update(void)
{
	cos_mpd_cntl(COS_MPD_UPDATE, 0, 0);
}

static inline long cos_spd_id(void)
{
	return cos_comp_info.cos_this_spd_id;
}

static inline void *cos_get_heap_ptr(void)
{
	return (void*)cos_comp_info.cos_heap_ptr;
}

static inline void cos_set_heap_ptr(void *addr)
{
	cos_comp_info.cos_heap_ptr = (vaddr_t)addr;
}

static inline char *cos_init_args(void)
{
	return cos_comp_info.init_string;
}

#define COS_EXTERN_FN(fn) __cos_extern_##fn

static inline long cos_cmpxchg(volatile void *memory, long anticipated, long result)
{
	long ret;

	__asm__ __volatile__(
		"call cos_atomic_cmpxchg"
		: "=d" (ret)
		: "a" (anticipated), "b" (memory), "c" (result)
		: "cc", "memory");

	return ret;
}

/* A uni-processor variant with less overhead but that doesn't
 * guarantee atomicity across cores. */
static inline int 
cos_cas_up(unsigned long *target, unsigned long cmp, unsigned long updated)
{
	char z;
	__asm__ __volatile__("cmpxchgl %2, %0; setz %1"
			     : "+m" (*target),
			       "=a" (z)
			     : "q"  (updated),
			       "a"  (cmp)
			     : "memory", "cc");
	return (int)z;
}

static inline void *cos_get_prealloc_page(void)
{
	char *h;
	long r;
	do {
		h = (void*)cos_comp_info.cos_heap_alloc_extent;
		if (!h || (char*)cos_comp_info.cos_heap_allocated >= h) return NULL;
		r = (long)h+PAGE_SIZE;
	} while (cos_cmpxchg(&cos_comp_info.cos_heap_allocated, (long)h, r) != r);

	return h;
}

/* allocate and release a page in the vas */
extern void *cos_get_vas_page(void);
extern void cos_release_vas_page(void *p);

/* only if the heap pointer is pre_addr, set it to post_addr */
static inline void 
cos_set_heap_ptr_conditional(void *pre_addr, void *post_addr)
{
	cos_cmpxchg(&cos_comp_info.cos_heap_ptr, (long)pre_addr, (long)post_addr);
}

/* from linux source in string.h */
static inline void *
cos_memcpy(void * to, const void * from, int n)
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

static inline void *
cos_memset(void * s, char c , int count)
{
	int d0, d1;
	__asm__ __volatile__(
		"rep\n\t"
		"stosb"
		: "=&c" (d0), "=&D" (d1)
		:"a" (c),"1" (s),"0" (count)
		:"memory");
	return s;
}

/* compiler branch prediction hints */
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define CFORCEINLINE __attribute__((always_inline))

/* 
 * A composite constructor (deconstructor): will be executed before
 * other component execution (after component execution).  CRECOV is a
 * function that should be called if one of the depended-on components
 * has failed (e.g. the function serves as a callback notification).
 */
#define CCTOR __attribute__((constructor))
#define CDTOR __attribute__((destructor)) /* currently unused! */
#define CRECOV(fnname) long crecov_##fnname##_ptr __attribute__((section(".crecov"))) = (long)fnname

static inline void
section_fnptrs_execute(long *list)
{
	int i;

	for (i = 0 ; i < list[0] ; i++) {
		typedef void (*ctors_t)(void);
		ctors_t ctors = (ctors_t)list[i+1];
		ctors();
	}
}

static void 
constructors_execute(void)
{
	extern long __CTOR_LIST__;
	section_fnptrs_execute(&__CTOR_LIST__);
}
static void 
destructors_execute(void)
{
	extern long __DTOR_LIST__;
	section_fnptrs_execute(&__DTOR_LIST__);
}
static void 
recoveryfns_execute(void)
{
	extern long __CRECOV_LIST__;
	section_fnptrs_execute(&__CRECOV_LIST__);
}

#define FAIL() *(int*)NULL = 0
static inline int cos_argreg_buff_intern(char *buff, int sz) { FAIL(); return 0; }
static inline void cos_argreg_init(void) { FAIL(); }
static inline void *cos_argreg_alloc(int sz) { FAIL(); return NULL; }
static inline int cos_argreg_free(void *p) { FAIL(); return 0; };
struct cos_array { char *mem; int sz; };
static inline int cos_argreg_arr_intern(struct cos_array *ca) { FAIL(); return 0; }

#define prevent_tail_call(ret) __asm__ ("" : "=r" (ret) : "m" (ret))
#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

#ifndef STR
#define STRX(x) #x
#define STR(x) STRX(x)
#endif

struct __thd_init_data {
	void *fn;
	void *data;
};

#endif
