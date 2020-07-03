#define LINUX_TEST
//#define CRINGBUF_DEBUG
#define CRINGBUF_CAUTIOUS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cringbuf.h>

#define ITER 1000000

#define BUF_SZ 40
struct cringbuf rb;
char            buffer[BUF_SZ];

void
populate(char *b, int sz)
{
	static int val = 0;
	int        i;

	for (i = 0; i < sz; i++) {
		b[i] = val;
		val  = (val + 1) % 127;
	}
	//	printf("populate wrote up to %d\n", val);
}

int
check(char *b, int sz)
{
	static int val = 0;
	int        i;

	//	printf("check: ");
	for (i = 0; i < sz; i++) {
		//		printf("%d ", b[i]);

		if (b[i] != val) return -1;
		val = (val + 1) % 127;
	}
	//	printf("\n");
	return 0;
}

void
consume_all(struct cringbuf *rb)
{
	char b[BUF_SZ];
	int  amnt = cringbuf_sz(rb);

	assert(cringbuf_consume(rb, b, amnt) == amnt);
	assert(cringbuf_sz(rb) == 0);
	assert(cringbuf_empty_sz(rb) == rb->sz - 1);
}

void
produce_all(struct cringbuf *rb)
{
	char b[BUF_SZ];
	int  amnt = cringbuf_empty_sz(rb);

	assert(cringbuf_produce(rb, b, amnt) == amnt);
	assert(cringbuf_empty_sz(rb) == 0);
	assert(cringbuf_sz(rb) == rb->sz - 1);
}

int
main(void)
{
	int  tot = BUF_SZ - sizeof(struct __cringbuf) - 1, left = tot, i, amnt, l;
	char b[BUF_SZ];

	cringbuf_init(&rb, buffer, BUF_SZ);

	assert(!cringbuf_active_extent(&rb, &l, tot));
	assert(cringbuf_sz(&rb) == 0 && cringbuf_empty_sz(&rb) == tot);
	for (i = 0; i < ITER; i++) {
		int l, p;

		amnt = (rand() % (left - 1)) + 1;
		//		printf("alloc %d, left %d, size %d, to increase %d, l %d\n", cringbuf_sz(&rb),
		// cringbuf_empty_sz(&rb), rb.sz, amnt, left);
		assert((rb.sz - cringbuf_sz(&rb)) >= amnt);
		populate(b, amnt);
		p = cringbuf_produce(&rb, b, amnt);
		assert(p >= 0);
		left -= amnt;
		//		printf("p %d->%d, s %d\n", amnt, p, cringbuf_sz(&rb));
		assert(left == cringbuf_empty_sz(&rb));

		amnt = (rand() % ((tot - left) + 2)) + 1;
		l    = cringbuf_consume(&rb, b, amnt);
		assert(l);
		assert(!check(b, l));
		left += l;
		//		printf("c %d->%d, s %d\n", amnt, l, cringbuf_sz(&rb));
		assert(left == cringbuf_empty_sz(&rb));
	}
	/* boundary case checking: */
	consume_all(&rb);
	produce_all(&rb);
	consume_all(&rb);

	return 0;
}
