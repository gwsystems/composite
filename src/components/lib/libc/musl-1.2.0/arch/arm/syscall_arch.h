#define __SYSCALL_LL_E(x) \
((union { long long ll; long l[2]; }){ .ll = x }).l[0], \
((union { long long ll; long l[2]; }){ .ll = x }).l[1]
#define __SYSCALL_LL_O(x) 0, __SYSCALL_LL_E((x))

#ifdef __thumb__

/* Avoid use of r7 in asm constraints when producing thumb code,
 * since it's reserved as frame pointer and might not be supported. */
#define __ASM____R7__
#define __asm_syscall(...) do { \
	__asm__ __volatile__ ( "mov %1,r7 ; mov r7,%2 ; svc 0 ; mov r7,%1" \
	: "=r"(r0), "=&r"((int){0}) : __VA_ARGS__ : "memory"); \
	return r0; \
	} while (0)

#else

#define __ASM____R7__ __asm__("r7")
#define __asm_syscall(...) do { \
	__asm__ __volatile__ ( "svc 0" \
	: "=r"(r0) : __VA_ARGS__ : "memory"); \
	return r0; \
	} while (0)
#endif

/* For thumb2, we can allow 8-bit immediate syscall numbers, saving a
 * register in the above dance around r7. Does not work for thumb1 where
 * only movs, not mov, supports immediates, and we can't use movs because
 * it doesn't support high regs. */
#ifdef __thumb2__
#define R7_OPERAND "rI"(r7)
#else
#define R7_OPERAND "r"(r7)
#endif

extern long __cos_syscall(int, long, long, long, long, long, long, long);

static inline long __syscall0(long n)
{
	return __cos_syscall(n, 0, 0, 0, 0, 0, 0, 0);
}

static inline long __syscall1(long n, long a)
{
	return __cos_syscall(n, a, 0, 0, 0, 0, 0, 0);
}

static inline long __syscall2(long n, long a, long b)
{
	return __cos_syscall(n, a, b, 0, 0, 0, 0, 0);
}

static inline long __syscall3(long n, long a, long b, long c)
{
	return __cos_syscall(n, a, b, c, 0, 0, 0, 0);
}

static inline long __syscall4(long n, long a, long b, long c, long d)
{
	return __cos_syscall(n, a, b, c, d, 0, 0, 0);
}

static inline long __syscall5(long n, long a, long b, long c, long d, long e)
{
	return __cos_syscall(n, a, b, c, d, e, 0, 0);
}

static inline long __syscall6(long n, long a, long b, long c, long d, long e, long f)
{
	return __cos_syscall(n, a, b, c, d, e, f, 0);
}

#define SYSCALL_FADVISE_6_ARG

#define SYSCALL_IPC_BROKEN_MODE
