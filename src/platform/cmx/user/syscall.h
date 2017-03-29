/* temporary */
static inline
int call_cap_asm(u32_t cap_no, u32_t op, int arg1, int arg2, int arg3, int arg4)
{
	long fault = 0;
	int ret=-1;

	cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;
	cap_no += op;

	/* put all these into registers and make the system call, using svc. after this, return the value needed */
	__asm__ __volatile__("ldr r0,%[_cap_no]  \n\t" \
			     "ldr r1,%[_arg1] \n\t" \
			     "ldr r2,%[_arg2] \n\t" \
			     "ldr r3,%[_arg3] \n\t" \
			     "ldr r4,%[_arg4] \n\t" \
			     "svc 0\n\t" \
			     "mov %[_ret],r5 \n\t" \
			     "mov %[_fault],r6 \n\t" \
			     : [_ret]"=r"(ret),[_fault]"=r"(fault) \
			     : [_cap_no]"m"(cap_no), [_arg1]"m"(arg1), [_arg2]"m"(arg2), [_arg3]"m"(arg3), [_arg4]"m"(arg4) \
			     : "memory", "r0", "r1", "r2", "r3", "r4", "r5", "r6", "cc");
	return ret;
}

static inline
int call_cap_retvals_asm(u32_t cap_no, u32_t op, int arg1, int arg2, int arg3, int arg4,
			 unsigned long *r1, unsigned long *r2)
{
	long fault = 0;
	int ret;

	cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;
	cap_no += op;

	/* put all these into registers and make the system call, using svc. after this, return the value needed */
	__asm__ __volatile__(".syntax unified \n\t" \
			     "ldr r0,%[_cap_no]  \n\t" \
			     "ldr r1,%[_arg1] \n\t" \
			     "ldr r2,%[_arg2] \n\t" \
			     "ldr r3,%[_arg3] \n\t" \
			     "ldr r4,%[_arg4] \n\t" \
			     "svc 0\n\t" \
			     "mov %[_ret],r5 \n\t" \
			     "mov %[_fault],r6 \n\t"
			     "mov %[_r1],r7 \n\t"
			     "mov %[_r2],r8 \n\t"
			     : [_ret]"=r"(ret),[_fault]"=r"(fault), [_r1]"=r"(r1), [_r2]"=r"(r2) \
			     : [_cap_no]"m"(cap_no), [_arg1]"m"(arg1), [_arg2]"m"(arg2), [_arg3]"m"(arg3), [_arg4]"m"(arg4) \
			     : "memory", "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "cc");
	return ret;
}
