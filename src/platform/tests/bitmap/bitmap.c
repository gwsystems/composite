#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

#define LINUX_TEST
#include <bitmap.h>

#define NTESTS 4096

int
main(void)
{
	u32_t bs[] = {0x0,    0x1,     0x2,    0x3,    0x4,    0x5,    0x6,    0x7,    0x8,
	              0x9,    0xa,     0xb,    0xc,    0xd,    0xe,    0xf,    0x10,   0xFFFFFFFF,
	              0x8888, 0xFF00,  0xF0F0, 0x0F0F, 0x00FF, 0x1111, 0x0101, 0x1010, 0xFFFF,
	              0xFFFE, 0x10000, 4095,   4096,   4097,   8192,   0,      0};
	int   i;

	for (i = 0; bs[i] || bs[i + 1]; i++) {
		printf("%x:  ones %d, nlpow2 %d, ls_one %d, _log32 %d, "
		       "log32 %d, lsorder %d, log32up %d\n",
		       bs[i], ones(bs[i]), nlpow2(bs[i]), ls_one(bs[i]), _log32(bs[i]), log32(bs[i]),
		       _log32(ls_one(bs[i])), log32up(bs[i]));
	}

	return 0;
}
