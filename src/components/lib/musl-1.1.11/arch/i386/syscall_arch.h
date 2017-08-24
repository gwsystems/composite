#define __SYSCALL_LL_E(x) \
((union { long long ll; long l[2]; }){ .ll = x }).l[0], \
((union { long long ll; long l[2]; }){ .ll = x }).l[1]
#define __SYSCALL_LL_O(x) __SYSCALL_LL_E((x))

extern __attribute__((regparm(1))) long
__cos_syscall(int, long, long, long, long, long, long, long);

static inline long __syscall0(long n)
{
	return __cos_syscall(n, 0, 0, 0, 0, 0, 0, 0);
	/*unsigned long __ret;
	__asm__ __volatile__ (".hidden __vsyscall ; call __vsyscall" : "=a"(__ret) : "a"(n) : "memory");
	return __ret;*/
}

static inline long __syscall1(long n, long a1)
{
	return __cos_syscall(n, a1, 0, 0, 0, 0, 0, 0);
	/*unsigned long __ret;
	__asm__ __volatile__ (".hidden __vsyscall ; call __vsyscall" : "=a"(__ret) : "a"(n), "d"(a1) : "memory");
	return __ret;*/
}

static inline long __syscall2(long n, long a1, long a2)
{
	return __cos_syscall(n, a1, a2, 0, 0, 0, 0, 0);
	/*unsigned long __ret;
	__asm__ __volatile__ (".hidden __vsyscall ; call __vsyscall" : "=a"(__ret) : "a"(n), "d"(a1), "c"(a2) : "memory");
	return __ret;*/
}

static inline long __syscall3(long n, long a1, long a2, long a3)
{
	return __cos_syscall(n, a1, a2, a3, 0, 0, 0, 0);
	/*unsigned long __ret;
	__asm__ __volatile__ (".hidden __vsyscall ; call __vsyscall" : "=a"(__ret) : "a"(n), "d"(a1), "c"(a2), "D"(a3) : "memory");
	return __ret;*/
}

static inline long __syscall4(long n, long a1, long a2, long a3, long a4)
{
	return __cos_syscall(n, a1, a2, a3, a4, 0, 0, 0);
	/*unsigned long __ret;
	__asm__ __volatile__ (".hidden __vsyscall ; call __vsyscall" : "=a"(__ret) : "a"(n), "d"(a1), "c"(a2), "D"(a3), "S"(a4) : "memory");
	return __ret;*/
}

static inline long __syscall5(long n, long a1, long a2, long a3, long a4, long a5)
{
	return __cos_syscall(n, a1, a2, a3, a4, a5, 0, 0);
	/*unsigned long __ret;
	__asm__ __volatile__ ("push %6 ; .hidden __vsyscall ; call __vsyscall ; add $4,%%esp" : "=a"(__ret) : "a"(n), "d"(a1), "c"(a2), "D"(a3), "S"(a4), "g"(a5) : "memory");
	return __ret;*/
}

static inline long __syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6)
{
	return __cos_syscall(n, a1, a2, a3, a4, a5, a6, 0);
	/*unsigned long __ret;
	__asm__ __volatile__ ("push %6 ; .hidden __vsyscall6 ; call __vsyscall6 ; add $4,%%esp" : "=a"(__ret) : "a"(n), "d"(a1), "c"(a2), "D"(a3), "S"(a4), "g"(0+(long[]){a5, a6}) : "memory");
	return __ret;*/
}
