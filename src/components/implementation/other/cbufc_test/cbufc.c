#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <timed_blk.h>
#include <cbuf_test.h>
#include <cbuf.h>

#define SZ 2048

void make_alloc_call_free(int sz, char c)
{
	void *m;
	cbuf_t cb = cbuf_null();
	u32_t id, idx;
	u64_t start, end;
	const int ITER = 1;
	int i;

	m = cbuf_alloc(sz, &cb);
	cbuf_unpack(cb, &id, &idx);
	printc("Now @ %p, memid %x, idx %x\n", m, id, idx);
	/* printc("cb is %d\n",cb); */
	memset(m, c, sz);
	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) {
		f(cb, sz);
	}
	rdtscll(end);
	printc("AVG: %lld\n", (end-start)/ITER);
	printc("initial %c, after %c\n", c, ((char *)m)[0]);
	cbuf_free(m);

	return;
}


static int create_thd(const char *pri)
{
	struct cos_array *data;
	int event_thd;
	int sz = strlen(pri) + 1;
    
	data = cos_argreg_alloc(sizeof(struct cos_array) + sz);
	assert(data);
	strcpy(&data->mem[0], pri);
	//data->sz = 4;
	data->sz = sz;

	if (0 > (event_thd = sched_create_thread(cos_spd_id(), data))) assert(0);
	cos_argreg_free(data);
    
	return event_thd;
}

void cos_init(void)
{
	void *mem1, *mem2;
	cbuf_t cb1 = cbuf_null(), cb2 = cbuf_null();
	u32_t id, idx;

	u64_t start, end;
	int i, j;
   	static int first = 0;
	static int hthd;
	static int lthd;
	/* timed_event_block(cos_spd_id(), 9); */
	if(first == 0){
		hthd = create_thd("r-1");
		lthd = cos_get_thd_id();

		first = 1;
	}

	printc("start alloc!\n");

	for (i=0;i<1;i++){
		mem1 = cbuf_alloc(2048, &cb1);
		cbuf_unpack(cb1, &id, &idx);
		printc("@ %p, memid %x, idx %x\n", mem1, id, idx);
		/* cbuf_free(mem1); */
	}

	/* make_alloc_call_free(SZ, 'a'); */

	/* cbuf_free(mem1); */
	/* cbuf_free(mem2); */

	return;

	cbuf_t cb3 = cbuf_null(), cb4 = cbuf_null();
	mem1 = cbuf_alloc(2048, &cb3);
	cbuf_unpack(cb3, &id, &idx);
	printc("@ %p, memid %x, idx %x\n", mem1, id, idx);
	mem2 = cbuf_alloc(SZ, &cb4);
	cbuf_unpack(cb4, &id, &idx);
	printc("@ %p, memid %x, idx %x\n", mem2, id, idx);

	/* make_alloc_call_free(SZ, 'a'); */

	cbuf_free(mem1);
	cbuf_free(mem2);

	return;
}

void bin(void)
{
	sched_block(cos_spd_id(), 0);
}
