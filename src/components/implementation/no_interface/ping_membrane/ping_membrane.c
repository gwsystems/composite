#include <cos_component.h>
#include <print.h>

#include <sched.h>
#include <membrane_meas.h>
 
#define ITER (1024*1024/64)

struct cache_line {
	struct cache_line *next;
	int data;
} CACHE_ALIGNED;
struct cache_line cache[ITER];

u64_t meas[0];

void init_cache(void)
{
	struct cache_line *l = cache, *temp;
	int i,j;
	unsigned long long random;
	l->data = 1;
	for (i = 0; i < ITER - 1; i++) {
		rdtscll(random);
		random /= 4;
		temp = &cache[random % ITER];
		if (temp->data == 0) {
			l->next = temp;
			l = l->next;
			l->data = 1;
		} else {
			i--;
			continue;
		}
	}
	for (i = 0; i < ITER; i++) {
		assert(cache[i].data);
	}
}
#define ITER2 1000
unsigned long cost[ITER2];

void cos_init(void)
{
	u64_t start, end, avg, tot = 0, dev = 0, sum = 0, sum2 = 0;
	int i, j, k, outlier;

	static int first = 1;
	
	if (first) {
		union sched_param sp;
		first = 0;
		init_cache();
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 20;
		if (sched_create_thd(cos_spd_id(), sp.v, 0, 0) == 0) BUG();
		return;
	}
//#define MPD_ENABLE
#ifdef MPD_ENABLE
	int c0[] = {10, 11, 0}, c1[] = {0}, c2[] = {0}, c3[] = {0},
	     c_last[] = {0};	
	int *ms[] = {c0, c1, c2, c3, c_last};

	for (j = 0 ; ms[j][0] ; j++) {
		for (i = 1 ; ms[j][i] != 0 ; i++) {
			if (cos_mpd_cntl(COS_MPD_MERGE, ms[j][0], ms[j][i])) {
				printc("merge of %d and %d failed. %d\n", ms[j][0], ms[j][i], 0);
			}
		}
	}
	printc("mpd done.\n");
#endif
	
	call_server(0,99,99,99);			/* get stack */
//	printc("addr %d, %d\n", &cache[1], &cache[0]);
	printc("optimal\n");
	for (i = 16; i <= ITER; i*= 2) {
		sum = sum2 = 0;
		outlier = 0;
		for (k = 0; k < ITER2; k++) {
			rdtscll(start);
			struct cache_line *node = cache;
			for (j = 0; j < i; j++) {
				node->data++;
				node = node->next;
			}
			rdtscll(end);
			cost[k] = (end - start);
			sum+= (end - start);
			//call_server(i, 0, 0, 0);
		}

		for (k = 0; k < ITER2; k++) { // clean cache....
			struct cache_line *node = cache;
			for (j = 0; j < ITER; j++) {
				node->data++;
				node = node->next;
			}
		}
		
		avg = sum / ITER2;
		for (k = 0; k < ITER2; k++) {
			if (cost[k] <= 2*avg) {
				sum2 += cost[k];
			} else
				outlier++;
		}
		printc("Core %ld, calling side, cache working set size %d, avg execution time %llu w/o %d outliers\n", cos_cpuid(), i * 64, sum2 / (ITER2-outlier), outlier);
	}
	printc("\nsamecore\n");
	for (i = 16; i <= ITER; i*= 2) {
		sum = sum2 = 0;
		outlier = 0;
		for (k = 0; k < ITER2; k++) {
			rdtscll(start);
			struct cache_line *node = cache;
			for (j = 0; j < i; j++) {
				node->data++;
				node = node->next;
			}
			rdtscll(end);
			cost[k] = (end - start);
			sum+= (end - start);
			call_server(i, 0, 0, 0);
		}

		for (k = 0; k < ITER2; k++) { // clean cache....
			struct cache_line *node = cache;
			for (j = 0; j < ITER; j++) {
				node->data++;
				node = node->next;
			}
		}

		avg = sum / ITER2;
		for (k = 0; k < ITER2; k++) {
			if (cost[k] <= 2*avg) {
				sum2 += cost[k];
			} else
				outlier++;
		}
		printc("Core %ld, calling side, cache working set size %d, avg execution time %llu w/o %d outliers\n", cos_cpuid(), i * 64, sum2 / (ITER2-outlier), outlier);
	}
	printc("\nxcore\n");
	for (i = 16; i <= ITER; i*= 2) {
		sum = sum2 = 0;
		outlier = 0;
		for (k = 0; k < ITER2; k++) {
			rdtscll(start);
			struct cache_line *node = cache;
			for (j = 0; j < i; j++) {
				node->data++;
				node = node->next;
			}
			rdtscll(end);
			cost[k] = (end - start);
			sum+= (end - start);
			call_server_x(i, 0, 0, 0);
		}

		for (k = 0; k < ITER2; k++) { // clean cache....
			struct cache_line *node = cache;
			for (j = 0; j < ITER; j++) {
				node->data++;
				node = node->next;
			}
		}

		avg = sum / ITER2;
		for (k = 0; k < ITER2; k++) {
			if (cost[k] <= 2*avg) {
				sum2 += cost[k];
			} else
				outlier++;
		}
		printc("Core %ld, calling side, cache working set size %d, avg execution time %llu w/o %d outliers\n", cos_cpuid(), i * 64, sum2 / (ITER2-outlier), outlier);
	}

	printc("done.\n");

	/* printc("Core %ld: starting Invocations.\n", cos_cpuid()); */

	/* for (i = 0 ; i < ITER ; i++) { */
	/* 	rdtscll(start); */
	/* 	call_server(99,99,99,99); */
	/* 	rdtscll(end); */
	/* 	meas[i] = end-start; */
	/* } */

	/* for (i = 0 ; i < ITER ; i++) tot += meas[i]; */
	/* avg = tot/ITER; */
	/* printc("avg %lld\n", avg); */
	/* for (tot = 0, i = 0, j = 0 ; i < ITER ; i++) { */
	/* 	if (meas[i] < avg*2) { */
	/* 		tot += meas[i]; */
	/* 		j++; */
	/* 	} */
	/* } */
	/* printc("avg w/o %d outliers %lld\n", ITER-j, tot/j); */

	/* for (i = 0 ; i < ITER ; i++) { */
	/* 	u64_t diff = (meas[i] > avg) ?  */
	/* 		meas[i] - avg :  */
	/* 		avg - meas[i]; */
	/* 	dev += (diff*diff); */
	/* } */
	/* dev /= ITER; */
	/* printc("deviation^2 = %lld\n", dev); */
	
//	printc("%d invocations took %lld\n", ITER, end-start);
	return;
}
