#include <cos_component.h>
#include <print.h>
#include <stdlib.h>
#include <sched.h>
#include <micro_pong.h>
#include <cbuf.h>

#define ITER 2000
#define MAX_SZ 4096
#define NCBUF 100

// using threadpool_max

void cos_init(void)
{
	u64_t start, end, start_tmp, end_tmp;
	int i, k, prev_sz = 1;

	cbuf_t cbt[NCBUF];
	memset(cbt, 0 , NCBUF*sizeof(cbuf_t));
	void *mt[NCBUF];
	unsigned int sz[NCBUF];

	for (i = 0; i < NCBUF ; i++){
		cbt[i] = cbuf_null();
		sz[i] = 0;
	}

	printc("\nMICRO BENCHMARK TEST (PINGPONG WITH CBUF)\n");

        /* RDTSCLL */
	printc("\n<<< RDTSCLL MICRO-BENCHMARK TEST >>>\n");
	rdtscll(start_tmp);
	for (i = 0 ; i < ITER ; i++) {
		rdtscll(start);
	}
	rdtscll(end_tmp);
	printc("%d rdtscll avg %lld cycs\n", ITER, (end_tmp-start_tmp)/ITER);
	
        /* PINGPONG */
	printc("\n<<< PINGPONG MICRO-BENCHMARK TEST >>>\n");
	call();
	for (k = 0; k <10 ;k++){
		
		rdtscll(start);
		for (i = 0 ; i < ITER ; i++) {
			call();
		}
		rdtscll(end);
		printc("%d invs avg %lld cycs\n", ITER, (end-start)/ITER);
	}
	printc("<<< PINGPONG BENCHMARK TEST DONE >>>\n");

        /* CACHING */
	printc("\n<<< WARM UP CBUF CACHE.......");
	for (i = 0; i < NCBUF ; i++){
		prev_sz += 4;
		prev_sz &= PAGE_SIZE-1;
		sz[i] = prev_sz;		
		mt[i] = cbuf_alloc(sz[i], &cbt[i]);
	}

	for (i = 0; i < NCBUF ; i++){
		simple_call_buf2buf(cbt[i], sz[i]);
	}

	for (i = 0; i < NCBUF ; i++){
		cbuf_free(mt[i]);
	}
	printc(" Done! >>>\n");

        /* CBUF_ALLOC  */
	printc("\n<<< CBUF_ALLOC MICRO-BENCHMARK TEST >>>\n");
	rdtscll(start);
	for (i = 0; i < NCBUF ; i++){
		prev_sz += 4;
		prev_sz &= PAGE_SIZE-1;
		sz[i] = prev_sz;
		mt[i] = cbuf_alloc(sz[i], &cbt[i]); 
	}
	rdtscll(end);
	printc("%d alloc_cbuf %llu cycs\n", NCBUF, (end-start)/NCBUF);
	printc("<<< CBUF_ALLOC MICRO-BENCHMARK TEST DONE >>>\n");

        /* CBUF2BUF  */
	printc("\n<<< CBUF2BUF MICRO-BENCHMARK TEST >>>\n");
	for (i = 0; i < NCBUF ; i++){
		call_buf2buf(cbt[i], sz[i]);
	}
	printc("<<< CBUF2BUF MICRO-BENCHMARK TEST DONE >>>\n");

        /* CBUF_FREE  */
	printc("\n<<< CBUF_FREE MICRO-BENCHMARK TEST >>>\n");
	rdtscll(start);
	for (i = 0; i < NCBUF ; i++){
		cbuf_free(mt[i]);                
	}
	rdtscll(end);
	printc("%d free_cbuf %llu cycs avg\n", NCBUF, (end-start)/NCBUF);
	printc("<<< CBUF_FREE MICRO-BENCHMARK TEST DONE >>>\n");

        /* CBUF_ALLOC-CBUF2BUF-CBUF_FREE */
	printc("\n<<< CBUF_ALLOC-CBUF2BUF-CBUF_FREE MICRO-BENCHMARK TEST >>>\n");
	prev_sz += 4;
	prev_sz &= PAGE_SIZE-1;
	sz[0] = prev_sz;
	rdtscll(start);
	for (i = 0; i < ITER ; i++){
		mt[0] = cbuf_alloc(sz[0], &cbt[0]);
		simple_call_buf2buf(cbt[0], sz[0]);
		cbuf_free(mt[0]);
	}
	rdtscll(end);
	printc("%d alloc-cbuf2buf-free %llu cycles avg\n", ITER, (end-start)/ITER);

	printc("<<< CBUF_ALLOC-CBUF2BUF-CBUF_FREE MICRO-BENCHMARK TEST DONE >>>\n");

	printc("\nMICRO BENCHMARK TEST (PINGPONG WITH CBUF) DONE!\n\n");
	return;
}


