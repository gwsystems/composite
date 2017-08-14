#include "kernel.h"
#include "tss.h"
#include "chal_asm_inc.h"

struct tss tss;

#define BITMASK(SHIFT, CNT) (((1ul << (CNT)) - 1) << (SHIFT))
#define PGSHIFT 0                       /* Index of first offset bit. */
#define PGBITS 12                       /* Number of offset bits. */
#define PGSIZE (1 << PGBITS)            /* Bytes in a page. */
#define PGMASK BITMASK(PGSHIFT, PGBITS) /* Page offset bits (0:12). */

void
tss_init(void)
{
	u32_t esp;

	tss.ss0    = SEL_KDSEG;
	tss.bitmap = 0xdfff;
	tss.esp0   = (((u32_t)&esp & ~PGMASK) + PGSIZE - STK_INFO_OFF);
}
