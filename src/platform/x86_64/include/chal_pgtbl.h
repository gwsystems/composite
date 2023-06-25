#pragma once

/* These code below are for x86 specifically, only used in x86 chal */
typedef enum {
	X86_PGTBL_PRESENT    = 1,
	X86_PGTBL_WRITABLE   = 1 << 1,
	X86_PGTBL_USER       = 1 << 2,
	X86_PGTBL_WT         = 1 << 3, /* write-through caching */
	X86_PGTBL_NOCACHE    = 1 << 4, /* caching disabled */
	X86_PGTBL_ACCESSED   = 1 << 5,
	X86_PGTBL_MODIFIED   = 1 << 6,
	X86_PGTBL_SUPER      = 1 << 7, /* super-page (4MB on x86-32) */
	X86_PGTBL_GLOBAL     = 1 << 8,
	/* Composite defined bits next*/
	X86_PGTBL_COSFRAME   = 1 << 9,
	X86_PGTBL_COSKMEM    = 1 << 10, /* page activated as kernel object */
	X86_PGTBL_QUIESCENCE = 1 << 11,
	X86_PGTBL_PKEY0      = 1ul << 59, /* MPK key bits */
	X86_PGTBL_PKEY1      = 1ul << 60,
	X86_PGTBL_PKEY2      = 1ul << 61,
	X86_PGTBL_PKEY3      = 1ul << 62,

	X86_PGTBL_XDISABLE   = 1ul << 63,
	/* Flag bits done. */

	X86_PGTBL_USER_DEF   = X86_PGTBL_PRESENT | X86_PGTBL_USER | X86_PGTBL_ACCESSED | X86_PGTBL_MODIFIED | X86_PGTBL_WRITABLE,
	X86_PGTBL_INTERN_DEF = X86_PGTBL_USER_DEF,
	X86_PGTBL_USER_MODIFIABLE = X86_PGTBL_WRITABLE | X86_PGTBL_PKEY0 | X86_PGTBL_PKEY1 | X86_PGTBL_PKEY2 | X86_PGTBL_PKEY3 | X86_PGTBL_XDISABLE,
} pgtbl_flags_x86_t;
