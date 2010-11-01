#include <cos_component.h>
#include <print.h>
#include <sched.h>
//#include <cbuf_test.h>
#include <cbuf.h>

void make_alloc_free(int sz)
{
	void *m;
	cbuf_t cb;
	u32_t id, idx;
	
	m = cbuf_alloc(sz, &cb);
	cbuf_unpack(cb, &id, &idx);
	printc("@ %p, memid %x, idx %x\n", m, id, idx);
	cbuf_free(m);

	return;
}

void cos_init(void)
{
	void *mem1, *mem2;
	cbuf_t cb1, cb2;
	u32_t id, idx;

	mem1 = cbuf_alloc(2048, &cb1);
	cbuf_unpack(cb1, &id, &idx);
	printc("@ %p, memid %x, idx %x\n", mem1, id, idx);
	mem2 = cbuf_alloc(2000, &cb2);
	cbuf_unpack(cb2, &id, &idx);
	printc("@ %p, memid %x, idx %x\n", mem2, id, idx);

	make_alloc_free(2000);
	make_alloc_free(2048);
	make_alloc_free(2021);

	cbuf_free(mem1);
	cbuf_free(mem2);

	return;
}

void bin(void)
{
	sched_block(cos_spd_id(), 0);
}
