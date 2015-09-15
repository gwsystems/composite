__attribute__((weak, regparm(1))) long
__cos_syscall(int syscall_num, long a, long b, long c, long d, long e, long f)
{
        long *x = *(long*)0;
        volatile long y = *x;

        return y;
}
