#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <cbuf_test.h>
#include <cbuf.h>

#define SZ 2000

void make_alloc_call_free(int sz, char c)
{
	void *m;
	cbuf_t cb = cbuf_null();
	u32_t id, idx;
	u64_t start, end;
	const int ITER = 100000;
	int i;

	m = cbuf_alloc(sz, &cb);
	cbuf_unpack(cb, &id, &idx);
	printc("@ %p, memid %x, idx %x\n", m, id, idx);
//	memset(m, c, sz);
	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) {
		f(cb, sz);
	}
	rdtscll(end);
	printc("AVG: %lld\n", (end-start)/ITER);
//	printc("initial %c, after %c\n", c, ((char *)m)[0]);
	cbuf_free(m);

	return;
}

void cos_init(void)
{
	void *mem1, *mem2;
	cbuf_t cb1 = cbuf_null(), cb2 = cbuf_null();
	u32_t id, idx;

	mem1 = cbuf_alloc(2048, &cb1);
	cbuf_unpack(cb1, &id, &idx);
	printc("@ %p, memid %x, idx %x\n", mem1, id, idx);
	mem2 = cbuf_alloc(SZ, &cb2);
	cbuf_unpack(cb2, &id, &idx);
	printc("@ %p, memid %x, idx %x\n", mem2, id, idx);

	make_alloc_call_free(SZ, 'a');

	cbuf_free(mem1);
	cbuf_free(mem2);

	return;
}

void bin(void)
{
	sched_block(cos_spd_id(), 0);
}
