#include "kernel.h"
#include "tss.h"
#include "chal_asm_inc.h"

struct tss tss[NUM_CPU];
#define BITMASK(SHIFT, CNT) (((1ul << (CNT)) - 1) << (SHIFT))
#define PGSHIFT 0                       /* Index of first offset bit. */
#define PGBITS 12                       /* Number of offset bits. */
#define PGSIZE (1 << PGBITS)            /* Bytes in a page. */
#define PGMASK BITMASK(PGSHIFT, PGBITS) /* Page offset bits (0:30). */

struct kernel_stack_info kernel_stack_info[NUM_CPU];

void
tss_init(const cpuid_t cpu_id)
{
	u64_t rsp;

	tss[cpu_id].bitmap = 0xdfff;
	tss[cpu_id].rsp0   = (((u64_t)&rsp & ~PGMASK) + PGSIZE - STK_INFO_OFF);

	memset(&kernel_stack_info[cpu_id], 0, sizeof(struct kernel_stack_info));

	kernel_stack_info[cpu_id].kernel_stack_addr = tss[cpu_id].rsp0;
}
