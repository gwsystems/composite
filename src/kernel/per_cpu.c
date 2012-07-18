#include "include/per_cpu.h"

struct per_core_variables per_core[MAX_NUM_CPU];

struct thread *core_get_curr_thd_asm(void)
{
	return core_get_curr_thd();
}
