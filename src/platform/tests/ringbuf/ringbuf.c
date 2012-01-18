#define LINUX_TEST
//#define CRINGBUF_DEBUG
#define CRINGBUF_CAUTIOUS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cringbuf.h>

#define ITER 1000000

#define BUF_SZ 129
struct cringbuf rb;
char buffer[BUF_SZ];

void populate(char *b, int sz)
{
	int i;

	for (i = 0 ; i < sz ; i++) b[i] = '1';
}

int check(char *b, int sz)
{
	int i;

	for (i = 0 ; i < sz ; i++) if (!b[i]) return -1;
	return 0;
}

int main(void)
{
	int tot = BUF_SZ-sizeof(struct __cringbuf), left = tot, i, amnt, l;
	char b[BUF_SZ];

	cringbuf_init(&rb, buffer, BUF_SZ);

	assert(!cringbuf_active_extent(&rb, &l, tot));
	assert(cringbuf_sz(&rb) == 0);
	for (i = 0 ; i < ITER ; i++) {
		int l, p;

		amnt = (rand() % (left-1))+1;
//		printf("alloc %d, left %d, size %d, to increase %d, l %d\n", cringbuf_sz(&rb), (rb.sz - cringbuf_sz(&rb)), rb.sz, amnt, left);
		assert((rb.sz - cringbuf_sz(&rb)) >= amnt);
		populate(b, BUF_SZ);
		p = cringbuf_produce(&rb, b, amnt);
		assert(p >= 0);
		left -= amnt;
//		printf("p %d->%d, s %d\n", amnt, p, cringbuf_sz(&rb));
		assert(left == cringbuf_empty_sz(&rb));

		amnt = (rand() % ((tot - left) + 2)) + 1;
		l = cringbuf_consume(&rb, b, amnt);
		assert(l);
		assert(!check(b, l));
		left += l;
//		printf("c %d->%d, s %d\n", amnt, l, cringbuf_sz(&rb));
		assert(left == cringbuf_empty_sz(&rb));
	}
 
	return 0;
}
