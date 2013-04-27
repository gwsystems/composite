#include <sched_hier.h>
#include <cstub.h>

CSTUB_FN_ARGS_6(int, sched_child_get_evt, spdid_t, spdid, int, idle, int, wakediff, cevt_t, *t, unsigned short int, *tid, u32_t, *time_elapsed)
	int cevt_tid;

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
	        "movl %%esi, %%ebx\n\t"
	        "movl %%edi, %%edx\n\t"
		"jmp 3f\n\t"
		"2:\n\t"
		"popl %%ebp\n\t"
		"movl $1, %%ecx\n\t"
	        "movl %%esi, %%ebx\n\t"
	        "movl %%edi, %%edx\n\t"
		"3:"
	        : "=a" (ret), "=c" (fault), "=b" (cevt_tid), "=d" (*time_elapsed)
		: "a" (uc->cap_no), "b" (spdid), "S" (idle), "D" (wakediff)
		: "memory", "cc");
	*t   = cevt_tid >> 16;
	*tid = cevt_tid & 0xFFFF;
CSTUB_POST
