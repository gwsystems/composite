/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef COS_COMPONENT_ARM_H
#define COS_COMPONENT_ARM_H

#include <consts.h>
#include <cos_types.h>
#include <util.h>
#include <string.h>
#include <bitmap.h>
#include <ps_plat.h>

/*
 * dewarn: strtok_r()
 * reference: feature_test_macro() requirement: _SVID_SOURCE || _BSD_SOURCE || _POSIX_C_SOURCE >= 1 || _XOPEN_SOURCE || _POSIX_SOURCE
 */
extern char *strtok_r(char *str, const char *delim, char **saveptr);
void libc_init();
char *cos_initargs_tar();

/* temporary */
static inline int
call_cap_asm(u32_t cap_no, u32_t op, int arg1, int arg2, int arg3, int arg4)
{
	long fault = 0;
	int  ret;

	cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;
	cap_no += op;

	/* Pass parameters: r1,r2,r3,r4,r5, return in r0, r2, r3, r4 */
	__asm__ __volatile__("ldr r0,%[_cap_no] \n\t"
	                     "ldr r2,%[_arg1] \n\t"
	                     "ldr r3,%[_arg2] \n\t"
	                     "ldr r4,%[_arg3] \n\t"
	                     "ldr r5,%[_arg4] \n\t"
	                     "svc #0x00 \n\t"
	                     "str r0,%[_ret] \n\t"
	                     "str r1,%[_fault] \n\t"
	                     : [ _ret ] "=m"(ret), [ _fault ] "=m"(fault)
	                     : [ _cap_no ] "m"(cap_no), [ _arg1 ] "m"(arg1), [ _arg2 ] "m"(arg2), [ _arg3 ] "m"(arg3),
	                       [ _arg4 ] "m"(arg4)
	                     : "memory", "cc", "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "lr");

	return ret;
}

static inline int
call_cap_retvals_asm(u32_t cap_no, u32_t op, word_t arg1, word_t arg2, word_t arg3, word_t arg4, word_t *r1, word_t *r2, word_t *r3)
{
	long fault = 0;
	int  ret;

	cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;
	cap_no += op;

	/* Pass parameters: r1,r2,r3,r4,r5, return in r0, r2, r3, r4 */
	__asm__ __volatile__("ldr r1,%[_cap_no] \n\t"
	                     "ldr r2,%[_arg1] \n\t"
	                     "ldr r3,%[_arg2] \n\t"
	                     "ldr r4,%[_arg3] \n\t"
	                     "ldr r5,%[_arg4] \n\t"
	                     "svc #0x00 \n\t"
	                     "str r0,%[_ret] \n\t"
	                     "ldr r6,%[_r2] \n\t"
	                     "str r2,[r6] \n\t"
	                     "ldr r6,%[_r3] \n\t"
	                     "str r3,[r6] \n\t"
	                     "ldr r6,%[_r4] \n\t"
	                     "str r4,[r6] \n\t"
	                     : [ _ret ] "=m"(ret), [ _r2 ] "=m"(r1), [ _r3 ] "=m"(r2), [ _r4 ] "=m"(r3)
	                     : [ _cap_no ] "m"(cap_no), [ _arg1 ] "m"(arg1), [ _arg2 ] "m"(arg2), [ _arg3 ] "m"(arg3),
	                       [ _arg4 ] "m"(arg4)
	                     : "memory", "cc", "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r14");

	return ret;
}

static inline word_t
call_cap_2retvals_asm(u32_t cap_no, u32_t op, word_t arg1, word_t arg2, word_t arg3, word_t arg4, word_t *r1, word_t *r2)
{
	long   fault = 0;
	word_t ret;

	cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;
	cap_no += op;

	/* Pass parameters: r1,r2,r3,r4,r5, return in r0, r2, r3, r4 */
	__asm__ __volatile__("ldr r1,%[_cap_no] \n\t"
	                     "ldr r2,%[_arg1] \n\t"
	                     "ldr r3,%[_arg2] \n\t"
	                     "ldr r4,%[_arg3] \n\t"
	                     "ldr r5,%[_arg4] \n\t"
	                     "svc #0x00 \n\t"
	                     "str r0,%[_ret] \n\t"
	                     "ldr r6,%[_r2] \n\t"
	                     "str r2,[r6] \n\t"
	                     "ldr r6,%[_r3] \n\t"
	                     "str r3,[r6] \n\t"
	                     : [ _ret ] "=m"(ret), [ _r2 ] "=m"(r1), [ _r3 ] "=m"(r2)
	                     : [ _cap_no ] "m"(cap_no), [ _arg1 ] "m"(arg1), [ _arg2 ] "m"(arg2), [ _arg3 ] "m"(arg3),
	                       [ _arg4 ] "m"(arg4)
	                     : "memory", "cc", "r0", "r1", "r2", "r3", "r4", "r5", "r6");

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
call_cap_op(u32_t cap_no, u32_t op_code, int arg1, int arg2, int arg3, int arg4)
{
	return call_cap_asm(cap_no, op_code, arg1, arg2, arg3, arg4);
}

static void
cos_print(char *s, int len)
{
	// FIXME: casting from a pointer to an int can be lossy
	call_cap(PRINT_CAP_TEMP, (int) s, len, 0, 0);
}

/**
 * FIXME: Please remove this since it is no longer needed
 */

extern long stkmgr_stack_space[ALL_TMP_STACKS_SZ];
extern struct cos_component_information __cosrt_comp_info;

static inline long
get_stk_data(int offset)
{
	unsigned long curr_stk_pointer;

	curr_stk_pointer = (unsigned long)&curr_stk_pointer;
	/*
	 * We save the CPU_ID and thread id in the stack for fast
	 * access.  We want to find the struct cos_stk (see the stkmgr
	 * interface) so that we can then offset into it and get the
	 * cpu_id.  This struct is at the _top_ of the current stack,
	 * and cpu_id is at the top of the struct (it is a u32_t).
	 */
	return *(long *)((curr_stk_pointer & ~(COS_STACK_SZ - 1)) + COS_STACK_SZ - offset * sizeof(u32_t));
}

#define GET_CURR_CPU cos_cpuid()

static inline long
cos_cpuid(void)
{
#if NUM_CPU == 1
	return 0;
#endif
	/*
	 * see comments in the get_stk_data above.
	 */
	return get_stk_data(CPUID_OFFSET);
}

static inline coreid_t
cos_coreid(void)
{
	return (coreid_t)cos_cpuid();
}

static inline unsigned short int
cos_get_thd_id(void)
{
	/*
	 * see comments in the get_stk_data above.
	 */
	return get_stk_data(THDID_OFFSET);
}

static inline invtoken_t
cos_inv_token(void)
{
	return get_stk_data(INVTOKEN_OFFSET);
}

typedef u16_t cos_thdid_t;

static thdid_t
cos_thdid(void)
{
	return cos_get_thd_id();
}

#define ERR_THROW(errval, label) \
	do {                     \
		ret = errval;    \
		goto label;      \
	} while (0)

static inline long
cos_spd_id(void)
{
	return __cosrt_comp_info.cos_this_spd_id;
}

static inline compid_t
cos_compid(void)
{
	return cos_spd_id();
}

static inline int
cos_compid_uninitialized(void)
{
	return cos_compid() == 0;
}

static inline void
cos_compid_set(compid_t cid)
{
	__cosrt_comp_info.cos_this_spd_id = cid;
}

static inline void *
cos_get_heap_ptr(void)
{
	return (void *)__cosrt_comp_info.cos_heap_ptr;
}

static inline void
cos_set_heap_ptr(void *addr)
{
	__cosrt_comp_info.cos_heap_ptr = (vaddr_t)addr;
}

static inline char *
cos_init_args(void)
{
	return __cosrt_comp_info.init_string;
}

#define COS_CPUBITMAP_STARTTOK 'c'
#define COS_CPUBITMAP_ENDTOK   ","
#define COS_CPUBITMAP_LEN      (NUM_CPU)

static inline int
cos_args_cpubmp(u32_t *cpubmp, char *arg)
{
	char *tok1 = NULL, *tok2 = NULL;
	char res[COMP_INFO_INIT_STR_LEN] = { '\0' }, *rs = res;
	int i, len = 0;

	if (!arg || !cpubmp) return -EINVAL;
	strncpy(rs, arg, COMP_INFO_INIT_STR_LEN);
	if (!strlen(arg)) goto allset;
	while ((tok1 = strtok_r(rs, COS_CPUBITMAP_ENDTOK, &rs)) != NULL) {
		if (tok1[0] == COS_CPUBITMAP_STARTTOK) break;
	}
	/* if "c" tag is not present.. set the component to be runnable on all cores */
	if (!tok1) goto allset;
	if (strlen(tok1) != (COS_CPUBITMAP_LEN + 1)) return -EINVAL;

	tok2 = tok1 + 1;
	len = strlen(tok2);
	for (i = 0; i < len; i++) {
		if (tok2[i] == '1') bitmap_set(cpubmp, (len - 1 - i));
	}

	return 0;

allset:
	bitmap_set_contig(cpubmp, 0, NUM_CPU, 1);

	return 0;
}

static inline int
cos_init_args_cpubmp(u32_t *cpubmp)
{
	return cos_args_cpubmp(cpubmp, cos_init_args());
}

/* Basic assembly for Cortex-A */
/*
static inline unsigned long
cos_ldrexw(volatile unsigned long *addr)
{
        unsigned long result;
        __asm__ __volatile__ ("ldrex %0, %1" : "=r" (result) : "Q" (*addr) );
        return(result);
}

static inline unsigned long
cos_strexw(unsigned long value, volatile unsigned long *addr)
{
        unsigned long result;
        __asm__ __volatile__ ("strex %0, %2, %1" : "=&r" (result), "=Q" (*addr) : "r" (value) );
        return(result);
}


static inline void
cos_clrex(void)
{
        __asm__ __volatile__ ("clrex" ::: "memory");
}
*/

static inline long
cos_cmpxchg(unsigned long *target, unsigned long old, unsigned long updated)
{
	unsigned long oldval, res;

	do {
		oldval = cos_ldrexw(target);

		if (oldval == old) /* 0=succeeded, 1=failed */
			res = cos_strexw(updated, target);
		else {
			cos_clrex();
			return 0;
		}
	} while (res);

	return 1;
}

/* A uni-processor variant with less overhead but that doesn't
 * guarantee atomicity across cores. */
static inline int
cos_cas_up(unsigned long *target, unsigned long old, unsigned long updated)
{
	unsigned long oldval, res;

	do {
		oldval = cos_ldrexw(target);

		if (oldval == old) /* 0=succeeded, 1=failed */
			res = cos_strexw(updated, target);
		else {
			cos_clrex();
			return 0;
		}
	} while (res);

	return 1;
}

static inline void *
cos_get_prealloc_page(void)
{
	char *h;
	long  r;
	do {
		h = (char *)__cosrt_comp_info.cos_heap_alloc_extent;
		if (!h || (char *)__cosrt_comp_info.cos_heap_allocated >= h) return NULL;
		r = (long)h + PAGE_SIZE;
	} while (cos_cmpxchg(&__cosrt_comp_info.cos_heap_allocated, (long)h, r) != r);

	return h;
}

/* allocate and release a page in the vas */
extern void *cos_get_vas_page(void);
extern void  cos_release_vas_page(void *p);

/* only if the heap pointer is pre_addr, set it to post_addr */
static inline void
cos_set_heap_ptr_conditional(void *pre_addr, void *post_addr)
{
	cos_cmpxchg(&__cosrt_comp_info.cos_heap_ptr, (long)pre_addr, (long)post_addr);
}

/* from linux source in string.h */
static inline void *
cos_memcpy(void *to, const void *from, int n)
{
	memcpy(to, from, n);

	return (to);
}

static inline void *
cos_memset(void *s, char c, int count)
{
	memset(s, c, count);
	return s;
}

/* compiler branch prediction hints */
#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#define CFORCEINLINE __attribute__((always_inline))
#define CWEAKSYMB __attribute__((weak))
/*
 * Create a weak function alias called "aliasn" which aliases the
 * existing function "name".  Example: COS_FN_WEAKALIAS(read,
 * __my_read) will take your "__my_read" function and create "read" as
 * an a weak alias to it.
 */
#define COS_FN_WEAKALIAS(weak_alias, name)					\
	__typeof__(name) weak_alias __attribute__((weak, alias(STR(name))))

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

	for (i = 0; i < list[0]; i++) {
		typedef void (*ctors_t)(void);
		ctors_t ctors = (ctors_t)list[i + 1];
		ctors();
	}
}

static void
constructors_execute(void)
{
	extern long __CTOR_LIST__;
	extern long __INIT_ARRAY_LIST__;
	section_fnptrs_execute(&__CTOR_LIST__);
	section_fnptrs_execute(&__INIT_ARRAY_LIST__);
}
static void
destructors_execute(void)
{
	extern long __DTOR_LIST__;
	extern long __FINI_ARRAY_LIST__;
	section_fnptrs_execute(&__DTOR_LIST__);
	section_fnptrs_execute(&__FINI_ARRAY_LIST__);
}
static void
recoveryfns_execute(void)
{
	extern long __CRECOV_LIST__;
	section_fnptrs_execute(&__CRECOV_LIST__);
}

#define FAIL() *(int *)NULL = 0

struct cos_array {
	char *mem;
	int   sz;
}; /* TODO: remove */
#define prevent_tail_call(ret) __asm__("" : "=r"(ret) : "m"(ret))

#ifndef STR
#define STRX(x) #x
#define STR(x) STRX(x)
#endif

struct __thd_init_data {
	void *fn;
	void *data;
};

typedef u32_t cbuf_t; /* TODO: remove when we have cbuf.h */

#endif
