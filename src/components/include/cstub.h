#define CSTUB_PRE(name) \
{ \
        long ret, fault = 0; \
 \
	/* \
	 * cap#    -> eax \
	 * sp      -> ebp \
	 * 1st arg -> ebx \
	 * 2nd arg -> esi \
	 * 3rd arg -> edi \
	 * 4th arg -> edx \
	 */ \
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

#define CSTUB_POST \
 \
	return ret; \
}

#define CSTUB_0(name)		   \
__attribute__((regparm(1))) long name##_call(struct usr_inv_cap *uc) \
        	CSTUB_PRE(name)	   \
                : "a" (uc->cap_no) \
		: "ebx", "edx", "esi", "edi", "memory", "cc"); \
		CSTUB_POST

#define CSTUB_1(name, type1)							\
	__attribute__((regparm(1))) long name##_call(struct usr_inv_cap *uc, type1 first) \
        	CSTUB_PRE(name)	   \
	: "a" (uc->cap_no), "b" (first)		\
		: "edx", "esi", "edi", "memory", "cc"); \
		CSTUB_POST

#define CSTUB_2(name, type1, type2)						\
	__attribute__((regparm(1))) long name##_call(struct usr_inv_cap *uc, type1 first, type2 second) \
        	CSTUB_PRE(name)	   \
	: "a" (uc->cap_no), "b" (first), "S" (second)	\
		: "edx", "edi", "memory", "cc"); \
		CSTUB_POST

#define CSTUB_3(name, type1, type2, type3)					\
	__attribute__((regparm(1))) long name##_call(struct usr_inv_cap *uc, type1 first, type2 second, type3 third) \
        	CSTUB_PRE(name)	   \
	: "a" (uc->cap_no), "b" (first), "S" (second), "D" (third)	\
		: "edx", "memory", "cc"); \
		CSTUB_POST

#define CSTUB_4(name, type1, type2, type3, type4)				\
	__attribute__((regparm(1))) long name##_call(struct usr_inv_cap *uc, type1 first, type2 second, type3 third, type4 fourth) \
        	CSTUB_PRE(name)	   \
	: "a" (uc->cap_no), "b" (first), "S" (second), "D" (third), "d" (fourth) \
		: "memory", "cc"); \
		CSTUB_POST
