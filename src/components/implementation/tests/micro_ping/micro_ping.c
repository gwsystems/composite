#include <cos_component.h>
#include <print.h>
#include <stdlib.h> 		/* rand */
#include <sched.h>
#include <micro_pong.h>
#include <cbuf.h>

#define ITER 1000000
#define MAX_SZ 4096
#define NCBUF 100

void cos_init(void)
{
	u64_t start, end, start_tmp, end_tmp;
	int i, k;

	cbuf_t cbt[NCBUF];
	memset(cbt, 0 , NCBUF*sizeof(cbuf_t));
	void *mt[NCBUF];
	unsigned int sz[NCBUF];

	for (i = 0; i < NCBUF ; i++){
		cbt[i] = cbuf_null();
		sz[i] = 0;
	}

	printc("\nMicro Benchmark Test (pingpong with cbuf)\n");

        /* RDTSCLL */
	printc("\n<<< RDTSCLL Micro-Benchmark Test >>>\n");
	rdtscll(start_tmp);
	for (i = 0 ; i < ITER ; i++) {
		rdtscll(start);
	}
	rdtscll(end_tmp);
	printc("%d rdtscll %lld cycs\n", ITER, end_tmp-start_tmp);
	
        /* PINGPONG */
	printc("\n<<< PINGPONG Micro-Benchmark Test >>>\n");
	call();
	for (k = 0; k <5 ;k++){
		
		rdtscll(start);
		for (i = 0 ; i < ITER ; i++) {
			call();
		}
		rdtscll(end);
		printc("%d invs %lld cycs\n", ITER, end-start);
	}
	printc("<<< PingPong Benchmark Test Done >>>\n");

        /* CACHING */
	printc("\n<<< Warm Up CBUF Cache.......");
	for (i = 0; i < NCBUF ; i++){
		sz[i] = (rand() % MAX_SZ) + 1;		
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
	printc("\n<<< CBUF_ALLOC Micro-Benchmark Test >>>\n");
	for (i = 0; i < NCBUF ; i++){
		sz[i] = (rand() % MAX_SZ) + 1;
		rdtscll(start);
		mt[i] = cbuf_alloc(sz[i], &cbt[i]); 
		rdtscll(end);
		printc("%d alloc_cbuf %llu cycs\n", NCBUF, end-start);
	}
	printc("<<< CBUF_ALLOC Micro-Benchmark Test Done >>>\n");

        /* CBUF2BUF  */
	printc("\n<<< CBUF2BUF Micro-Benchmark Test >>>\n");
	for (i = 0; i < NCBUF ; i++){
		call_buf2buf(cbt[i], sz[i]);
	}
	printc("<<< CBUF2BUF Micro-Benchmark Test Done >>>\n");

        /* CBUF_FREE  */
	printc("\n<<< CBUF_FREE Micro-Benchmark Test >>>\n");
	for (i = 0; i < NCBUF ; i++){
		rdtscll(start);
		cbuf_free(mt[i]);                
		rdtscll(end);
		printc("%d free_cbuf %llu cycs\n", NCBUF, end-start);
	}
	printc("<<< CBUF_FREE Micro-Benchmark Test Done >>>\n");

        /* CBUF_ALLOC-CBUF2BUF-CBUF_FREE */
	printc("\n<<< CBUF_ALLOC-CBUF2BUF-CBUF_FREE Micro-Benchmark Test >>>\n");
	sz[0] = (rand() % MAX_SZ) + 1;
	rdtscll(start);
	for (i = 0; i < ITER ; i++){
		mt[0] = cbuf_alloc(sz[0], &cbt[0]);
		simple_call_buf2buf(cbt[0], sz[0]);
		cbuf_free(mt[0]);
	}
	rdtscll(end);
	printc("%d alloc-cbuf2buf-free %llu cycs\n", ITER, end-start);

	printc("<<< CBUF_ALLOC-CBUF2BUF-CBUF_FREE Micro-Benchmark Test Done >>>\n");

	printc("\nMicro Benchmark Test (pingpong with cbuf) done!\n\n");
	return;
}


