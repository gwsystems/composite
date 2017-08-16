#include <cos_component.h>

#include <print.h>

__attribute__((regparm(1))) void call_call(struct usr_inv_cap *uc)
{
	int ret, fault = 0;

	/* 
	 * cap#    -> eax
	 * sp      -> ebp
	 * 1st arg -> ebx
	 * 2nd arg -> esi
	 * 3rd arg -> edi
	 * 4th arg -> edx
	 */
	__asm__ __volatile__(
		"pushl %%ebp\n\t"
		"movl %%esp, %%ebp\n\t"
		"movl $1f, %%ecx\n\t"
		"sysenter\n\t"
		".align 8\n\t"
		"jmp 2f\n\t"
		".align 8\n\t"
		"1:\n\t"
		"popl %%ebp\n\t"
		"movl $0, %%ecx\n\t"
		"jmp 3f\n\t"
		"2:\n\t"
		"popl %%ebp\n\t"
		"movl $1, %%ecx\n\t"
		"3:"
		: "=a" (ret), "=c" (fault)
		: "a" (uc->cap_no)
		: "ebx", "edx", "esi", "edi", "memory", "cc");

	return;
}
