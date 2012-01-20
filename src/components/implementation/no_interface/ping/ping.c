#include <cos_component.h>
#include <print.h>

#include <sched.h>
#include <pong.h>
/* #include <pong_lower.h> */
#include <cbuf.h>

//#define CONTEXT_SWITCH
//#define MEASURE_PIP 
//#define MEASURE_CACHED_CBUF
#define MEASURE_CACHED_STK

#ifdef CONTEXT_SWITCH    // change to max_policy!!  use lubench2.sh
void cos_init(void)
{
	call();
	return;
}
#endif


#ifdef MEASURE_PIP   // use pool_1 for stk; use pool_1 with simple_stk for cbuf, use lubench2.sh
void cos_init(void)
{
	call();
	return;
}
#endif


#ifdef MEASURE_CACHED_STK   // also invocations measure, change to thread_pool_1 and use lubench1.sh
                            // also measure simple stack cost
#define ITER 1000000
void cos_init(void)
{
	u64_t start, end;
	int i;

	int k = 0;

	call();
	printc("Starting Invocations. %d\n",k);
 
	for (k = 0; k <20 ;k++){
		
		rdtscll(start);
		for (i = 0 ; i < ITER ; i++) {
			call();
		}
		rdtscll(end);
		
		printc("%d invs %lld cycs\n", ITER, end-start);
	}
	return;
}

#endif



#ifdef MEASURE_CACHED_CBUF   // In order to ensure enough cbufs, use max polocy
#define SZ 4096
#define NCBUF 50
void cos_init(void)
{
	u64_t start, end;
	int i;

	u32_t id, idx;

	cbuf_t cbt[NCBUF];
	memset(cbt, 0 , NCBUF*sizeof(cbuf_t));
	void *mt[NCBUF];

	for (i = 0; i < NCBUF ; i++){
		cbt[i] = cbuf_null();
	}

	printc("Starting Invocations.\n");


	for (i = 0; i < NCBUF ; i++){
		mt[i] = cbuf_alloc(SZ, &cbt[i]);
		memset(mt[i], 'a', SZ);
	}

	for (i = 0; i < NCBUF ; i++){
		call_buf2buf(cbt[i], SZ, 0);
	}

	for (i = 0; i < NCBUF ; i++){
		cbuf_free(mt[i]);
	}


        // >>>>>>> benchmark for alloc
	for (i = 0; i < NCBUF ; i++){
		rdtscll(start);
		mt[i] = cbuf_alloc(SZ, &cbt[i]); 
		rdtscll(end);
		printc("%d alloc_cbuf %llu cycs\n", NCBUF, end-start);
	}


	for (i = 0; i < NCBUF ; i++){
		cbuf_unpack(cbt[i], &id, &idx);
		memset(mt[i], 'a', SZ);
		call_buf2buf(cbt[i], SZ, 1);     // >>>>>>> benchmark for buf2buf
	}


        // >>>>>>> benchmark for free
	for (i = 0; i < NCBUF ; i++){
		rdtscll(start);
		cbuf_free(mt[i]);                
		rdtscll(end);
		printc("%d free_cbuf %llu cycs\n", NCBUF, end-start);
	}

	return;
}
#endif


