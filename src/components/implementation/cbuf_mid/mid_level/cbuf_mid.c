#include <cos_component.h>
#include <print.h>
#include <cbuf.h>
#include <cbuf_bot.h>

#define SZ 4096
#define iter 20

volatile unsigned long test;

#define DELAY 10000

void delay(int k)
{
	int i,j;
	test = 0;
	for (i = 0; i < k ; i++){
		for (j = 0; j < k ; j++){
			test++;
		}
	}

	return;
}

void cbuf_call(char c)
{

	printc("\n****** MID: thread %d in spd %ld ******\n",cos_get_thd_id(), cos_spd_id());

	/* cbuf_t cb = cbuf_null(); */
	/* u64_t start, end; */
	int i, id;

	cbuf_t cbt[iter];
	void *mt[iter];
	for (i = 0; i < iter ; i++){
		cbt[i] = cbuf_null();
		mt[i] = cbuf_alloc_ext(SZ, &cbt[i], CBUF_TMEM);
		cbuf_unpack(cbt[i], &id);
		printc("thread %d Now @ %p, memid %x\n", cos_get_thd_id(), mt[i], id);
		assert(mt[i]);
		memset(mt[i], c, SZ);
	}

	
	delay(DELAY);
	delay(DELAY);

	for (i = 0; i < iter ; i++){
		f(cbt[i],SZ);
	}


	/* delay(DELAY); */
	/* delay(DELAY); */

	printc("\n****** MID free: thread %d in spd %ld ******\n",cos_get_thd_id(), cos_spd_id());

	for (i = 0; i < iter ; i++){
		cbuf_free(cbt[i]);
	}

	delay(DELAY);

	/* check_val(); */
	/* check_val1(); */

	/* m = cbuf_alloc(SZ, &cb); */
	/* cbuf_unpack(cb, &id, &idx); */
	/* printc("....Now @ %p, memid %x, idx %x\n", m, id, idx); */
	/* /\* printc("cb is %d\n",cb); *\/ */
	/* memset(m, c, SZ); */
	/* rdtscll(start); */
	/* for (i = 0 ; i < ITER ; i++) { */
	/* 	f(cb, SZ); */
	/* } */
	/* rdtscll(end); */
	/* printc("AVG: %lld\n", (end-start)/ITER); */
	/* printc("initial %c, after %c\n", c, ((char *)m)[0]); */
	/* cbuf_free(m); */

	return;
}
