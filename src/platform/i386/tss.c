#include "kernel.h"
#include "tss.h"
#include "chal_asm_inc.h"

struct tss tss[NUM_CPU];

#define BITMASK(SHIFT, CNT) (((1ul << (CNT)) - 1) << (SHIFT))
#define PGSHIFT 0                       /* Index of first offset bit. */
#define PGBITS 12                       /* Number of offset bits. */
#define PGSIZE (1 << PGBITS)            /* Bytes in a page. */
#define PGMASK BITMASK(PGSHIFT, PGBITS) /* Page offset bits (0:12). */

void
tss_init(const cpuid_t cpu_id)
{
	u32_t esp;

	tss[cpu_id].ss0    = SEL_KDSEG;
	tss[cpu_id].bitmap = 0xdfff;
	tss[cpu_id].esp0   = (((u32_t)&esp & ~PGMASK) + PGSIZE - STK_INFO_OFF);
}
