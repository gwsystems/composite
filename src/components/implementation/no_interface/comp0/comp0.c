#include <cos_component.h>

#include <sched_hier.h>

int nothing = 0, ret = 0;
volatile int mem = 0;

int spd0_main(void)
{
        long fault = 0;
	u32_t cap_no = ((1<<COS_CAPABILITY_OFFSET));
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
		"3:"
		: "=a" (ret), "=c" (fault)
                : "a" (cap_no)
		: "ebx", "edx", "esi", "edi", "memory", "cc");

	if (mem) sched_init(); // we need to link the cap!

	return ret;
}
