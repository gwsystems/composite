#include "include/per_cpu.h"
#include "include/thread.h"
struct per_core_variables per_core[NUM_CPU];

#define COS_SYSCALL __attribute__((regparm(0)))

COS_SYSCALL __attribute__((cdecl)) struct thread *
core_get_curr_thd_asm(void)
{
	return core_get_curr_thd();
}
