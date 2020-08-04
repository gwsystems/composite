#define __SYSCALL_LL_E(x) \
((union { long long ll; long l[2]; }){ .ll = x }).l[0], \
((union { long long ll; long l[2]; }){ .ll = x }).l[1]
#define __SYSCALL_LL_O(x) __SYSCALL_LL_E((x))

#if SYSCALL_NO_TLS
#define SYSCALL_INSNS "int $128"
#else
#define SYSCALL_INSNS "call *%%gs:16"
#endif

#define SYSCALL_INSNS_12 "xchg %%ebx,%%edx ; " SYSCALL_INSNS " ; xchg %%ebx,%%edx"
#define SYSCALL_INSNS_34 "xchg %%ebx,%%edi ; " SYSCALL_INSNS " ; xchg %%ebx,%%edi"

extern __attribute__((regparm(1))) long __cos_syscall(int, long, long, long, long, long, long, long);

static inline long __syscall0(long n)
{
	return __cos_syscall(n, 0, 0, 0, 0, 0, 0, 0);
}

static inline long __syscall1(long n, long a1)
{
	return __cos_syscall(n, a1, 0, 0, 0, 0, 0, 0);
}

static inline long __syscall2(long n, long a1, long a2)
{
	return __cos_syscall(n, a1, a2, 0, 0, 0, 0, 0);
}

static inline long __syscall3(long n, long a1, long a2, long a3)
{
	return __cos_syscall(n, a1, a2, a3, 0, 0, 0, 0);
}

static inline long __syscall4(long n, long a1, long a2, long a3, long a4)
{
	return __cos_syscall(n, a1, a2, a3, a4, 0, 0, 0);
}

static inline long __syscall5(long n, long a1, long a2, long a3, long a4, long a5)
{
	return __cos_syscall(n, a1, a2, a3, a4, a5, 0, 0);
}

static inline long __syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6)
{
	return __cos_syscall(n, a1, a2, a3, a4, a5, a6, 0);
}

#define SYSCALL_USE_SOCKETCALL
