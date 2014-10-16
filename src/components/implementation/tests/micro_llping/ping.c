#include <cos_component.h>
#include <cos_debug.h>

#include <pong.h>

#include <stdio.h>
#include <string.h>

int
prints(char *str)
{
	return 0;
}

int __attribute__((format(printf,1,2))) 
printc(char *fmt, ...)
{
	char s[128];
	va_list arg_ptr;
	int ret, len = 128;

	va_start(arg_ptr, fmt);
	ret = vsnprintf(s, len, fmt, arg_ptr);
	va_end(arg_ptr);
	ret = cos_print(s, ret);

	return ret;
}

unsigned long long tsc_start(void)
{
	unsigned long cycles_high, cycles_low; 
	asm volatile ("movl $0, %%eax\n\t"
		      "CPUID\n\t"
		      "RDTSCP\n\t"
		      "movl %%edx, %0\n\t"
		      "movl %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low) :: 
		      "%eax", "%ebx", "%ecx", "%edx");

	return ((unsigned long long)cycles_high << 32) | cycles_low;
}

static void
tlb_quiescence_wait(void)
{
	u64_t s,e;
	rdtscll(s);
	while (1) {
		rdtscll(e);
		if (QUIESCENCE_CHECK(e, s, TLB_QUIESCENCE_CYCLES)) break;
	}
}

int delay(unsigned long long cycles) {
	unsigned long long s,e;
	volatile int mem = 0;

	s = tsc_start();
	while (1) {
		e = tsc_start();
		if (e - s > cycles) return 0; // x us
		mem++;
	}

	return 0;
}

#define ITER (1024*1024)//(100*1000)

#include <ck_pr.h>
#define N_SYNC_CPU (NUM_CPU_COS)
int synced_nthd = 0;
void sync_all()
{
	int ret;

	ret = ck_pr_faa_int(&synced_nthd, 1);
	ret = (ret/N_SYNC_CPU + 1)*N_SYNC_CPU;
	while (ck_pr_load_int(&synced_nthd) < ret) ;
	
	return;
}

int all_exit = 0;
//#define MEAS_AVG
void pingpong(void)
{
	int i;
	long long diff;
	u64_t s, e, avg, avg2;
	u64_t sum = 0, max = 0, stddev_sum = 0;
	volatile u32_t last_tick, curr_tick;

//#ifdef MEAS_AVG
	printc("core %ld: doing pingpong IPC test\n", cos_cpuid());
	
	avg = 638; /* on the 40-core server.*/
	sum = 0;
	/////////////////
	last_tick = printc("FLUSH!!");
	/* printc("core %ld: doing pingpong w/ flush @tick %u!\n", cos_cpuid(), last_tick); */
	for (i = 0; i < ITER; i++) {
		s = tsc_start();
		call_cap(4, 0, 0, 0, 0);
		rdtscll(e);
//		if (e-s > 20000) printc("large: %llu @ %llu\n", e-s, e);
		curr_tick = printc("FLUSH!!");
		if (unlikely(curr_tick != last_tick)) {
//			printc("timer detected @ %llu, %d, %d, (cost %llu)\n", e, last_tick, curr_tick, e-s);
//			if (last_tick+1 != curr_tick) printc("tick diff > 1: %u, %u\n", last_tick,curr_tick);
			last_tick = curr_tick;
			i--;
			continue;
		}
		diff = e-s;
		sum += diff;

		diff = (diff-avg);
		stddev_sum += (diff*diff);

		if (max < e-s) max = e-s;
	}
	avg2 = sum /ITER;
	stddev_sum /= ITER;
	if (avg != avg2) printc(">>>>Warning: assumed average overhead not consistent with the measured number\n");
	printc("core %ld: @tick %u pingpong done, avg (%llu, %llu), max %llu, stddev^2 %llu\n", 
	       cos_cpuid(), curr_tick, avg, avg2, max, stddev_sum);

	////////////////////////////
	call_cap(4, 0, 0, 0, 0);
	rdtscll(s);
	for (i = 0; i < ITER; i++) {
		call_cap(4, 0, 0, 0, 0);
	}
	rdtscll(e);

	avg = (e-s)/ITER;
	printc("\n core %ld: %d IPCs done, avg cost %llu cycles (no interrupt filtering)\n\n", cos_cpuid(), ITER, avg);

	ck_pr_store_int(&all_exit, 1);

	return;
}

int arcv_ready[NUM_CPU];

#include <ck_pr.h>

struct record_per_core {
	int rcv;
	int snd_thd_created;
	char _pad[CACHE_LINE-sizeof(int)*2];
}CACHE_ALIGNED;

struct record_per_core received[NUM_CPU];

void rcv_thd(void)
{
	int ret;
	struct record_per_core *curr_rcv = &received[cos_cpuid()];

//	printc("core %ld: rcv thd %d ready in ping!\n", cos_cpuid(), cos_get_thd_id());

	while (1) {
		ret = call_cap(ACAP_BASE + captbl_idsize(CAP_ARCV)*cos_cpuid(),0,0,0,0);
		if (ret) {
			printc("ERROR: arcv ret %d", ret);
			printc("rcv thd %d switching back to alpha %ld!\n", 
			       cos_get_thd_id(), SCHED_CAPTBL_ALPHATHD_BASE + cos_cpuid()*captbl_idsize(CAP_THD));
			ret = cap_switch_thd(SCHED_CAPTBL_ALPHATHD_BASE + cos_cpuid()*captbl_idsize(CAP_THD));
		}
		ck_pr_store_int(&curr_rcv->rcv, curr_rcv->rcv + 1);
	}
}

char *shmem = (char *)(0x44c00000-PAGE_SIZE);

void ipi_test(void)
{
	volatile int curr_tick, last_tick;

	struct record_per_core *curr_rcv = &received[cos_cpuid()];
	int i, last = 0;
	int target = SND_RCV_OFFSET + cos_cpuid();
	u64_t s, e, s1, e1;
	volatile u64_t *pong_shmem = (u64_t *)&shmem[(target) * CACHE_LINE];
	u64_t sum = 0, sum2 = 0, max = 0, stddev = 0;
	u32_t avg;
	int diff;

	avg = 18122;//6366;//3050;
	while (ck_pr_load_int(&arcv_ready[target]) == 0) ;
//		printc("core %ld: start sending ipi\n", cos_cpuid());
	rdtscll(s);
	for (i = 0; i<ITER; i++) {
//			last = ck_pr_load_int(&curr_rcv->rcv);
		*pong_shmem = 0;
		cos_inst_bar();
		last_tick = printc("FLUSH!!");
		cos_inst_bar();
		s1 = tsc_start();
		call_cap(ACAP_BASE + captbl_idsize(CAP_ASND)*target, 0, 0, 0, 0);
		/* rdtscll(e1); */
		/* sum2 += e1-s1; */

		cos_inst_bar();

		e1 = 0;
		while (e1 == 0) { e1 = *pong_shmem; }
//			while (ck_pr_load_int(&curr_rcv->rcv) == last) ;
//			rdtscll(e1);
//			sum2 += e1-s1;

		e = tsc_start();
		cos_inst_bar();
		curr_tick = printc("FLUSH!!");
		cos_inst_bar();
		if (unlikely(curr_tick != last_tick)) {
			delay(10000);
//				printc("PING: timer detected @ %llu, %d, %d\n", e, last_tick, curr_tick);
			//if (last_tick+1 != curr_tick) printc("tick diff > 1: %u, %u\n", last_tick,curr_tick);
//				last_tick = curr_tick;
			i--;
			continue;
		}

		if (unlikely(e1 < s1)) {
//				printc("e1 %llu < s1 %llu\n", e1, s1);
			i--;
			continue;
		} else {
			diff = e1 < e? e1-s1 : e-s1;
			if (diff>50000) {
				printc("cpu %ld curr_tick %d\n", cos_cpuid(), curr_tick);
				i--; continue;
			}

			if (diff > (int)max) max = diff;

			sum += diff;
			diff -= avg;
			stddev += diff*diff;
		}
		/* wait for receiving side to get ready. */
		/* last = arcv_ready[target]; */
		/* while (ck_pr_load_int(&arcv_ready[target]) == last) ; */
	}
	rdtscll(e);
//		printc("core %ld: ipi avg ( %llu, %llu ): %llu\n", cos_cpuid(), (e-s)/ITER, sum2/ITER, sum/ITER);
	printc("core %ld: ipi avg (sender %llu): %llu, max %llu, stddev^2 %llu\n", cos_cpuid(), sum2/ITER, sum/ITER, max, stddev / ITER);
	if (cos_cpuid() == 0) ck_pr_store_int(&all_exit, 1);
	goto done;
done:
	return;
}

void cap_test(void)
{
	int i, curr_cpu, my_cap, ret, ret1;
	long long diff;
	u64_t s, e, avg, avg2;
	u64_t sum = 0, max = 0, stddev_sum = 0;
	volatile u32_t last_tick, curr_tick;
	int lid;

//#ifdef MEAS_AVG
	curr_cpu = cos_cpuid();

#define CAPTBL_LEAFSZ  16
	/* no false sharing. different pages to avoid prefetching */
	my_cap = PAGE_SIZE/2/CAPTBL_LEAFSZ*510 - curr_cpu*PAGE_SIZE/CAPTBL_LEAFSZ;
	lid = 64 + 4*curr_cpu;

	printc("core %d: doing cap map/unmap test @ cap %d, lid %d\n", curr_cpu, my_cap, lid);
	
	avg = 516; /* on the 40-core server.*/
	//avg = 404; /* on the 40-core server.*/

	sum = 0;
	/////////////////
	last_tick = printc("FLUSH!!");
	/* printc("core %ld: doing pingpong w/ flush @tick %u!\n", cos_cpuid(), last_tick); */
	for (i = 0; i < ITER; i++) {
		s = tsc_start();
		ret = call_cap_op(PING_CAPTBL, CAPTBL_OP_SINVACTIVATE,
				  my_cap, PING_COMPCAP, 123, 0);
		rdtscll(e);

		ret1 = call_cap_op(PING_CAPTBL, CAPTBL_OP_SINVDEACTIVATE,
				  my_cap, lid, 0, 0);

//		if (e-s > 20000) printc("large: %llu @ %llu\n", e-s, e);
		delay(KERN_QUIESCENCE_CYCLES);

		if (unlikely(ret || ret1)) printc("ACT/DEACT failed on core %d>>>>>>>>>>>>>> %d, %d\n", curr_cpu, ret, ret1);

		curr_tick = printc("FLUSH!!");
		if (unlikely(curr_tick != last_tick)) {
//			printc("timer detected @ %llu, %d, %d, (cost %llu)\n", e, last_tick, curr_tick, e-s);
//			if (last_tick+1 != curr_tick) printc("tick diff > 1: %u, %u\n", last_tick,curr_tick);
			last_tick = curr_tick;
			i--;
			continue;
		}
		diff = e-s;
		sum += diff;

		diff = (diff-avg);
		stddev_sum += (diff*diff);

		if (max < e-s) max = e-s;
	}
	avg2 = sum /ITER;
	stddev_sum /= ITER;
	if (avg != avg2) 
		printc(">>>>Warning: assumed average overhead not consistent with the measured number %llu, %llu\n", avg, avg2);

	printc("core %ld: @tick %u cap map/unmap done, avg %llu, max %llu, stddev^2 %llu\n", 
	       cos_cpuid(), curr_tick, avg2, max, stddev_sum);

	ck_pr_store_int(&all_exit, 1);

	return;
}

#define PING_MEM_START (0x44800000)

void mem_test(void)
{
	int n_loops, ii, j, curr_cpu, ret, ret1;
	unsigned long my_vaddr, start_vaddr, filter_out = 0, filter_out_b = 0;
	long long diff;
	u64_t s, e, avg, avg_unmap, avg2, avg2_unmap;
	u64_t sum = 0, max = 0, stddev_sum = 0, sum_unmap = 0, stddev_sum_unmap = 0, max_unmap = 0;
	volatile u32_t last_tick, curr_tick, tlb_tick;
	int lid;

//#ifdef MEAS_AVG
	curr_cpu = cos_cpuid();
	start_vaddr = 0x80000000 - (1+cos_cpuid())*0x400000; /* no false sharing. */
	lid = 64 + 4*curr_cpu;

//	printc("core %d: doing MEM map/unmap test\n", curr_cpu);
	
	//avg = 516;//1360; /* on the 40-core server.*/
	avg = 422;//1360; /* on the 40-core server.*/
	avg_unmap = 481; 

	sum = 0;
	/////////////////
	/* printc("core %ld: doing pingpong w/ flush @tick %u!\n", cos_cpuid(), last_tick); */

	n_loops = ITER/1024;
	if (ITER % 1024) printc(">>>>>>>>>>>>>>>>ITER (%d) should be multiple of 1024!\n", ITER);

	for (ii = 0; ii < n_loops; ii++) {
//		printc("i %d\n", ii);
		last_tick = printc("FLUSH!!");
		for (j = 0; j < 1024; j++) {
			my_vaddr = start_vaddr + j * PAGE_SIZE;
			s = tsc_start();
			ret = call_cap_op(PING_PGTBL, CAPTBL_OP_CPY,
					  PING_MEM_START, PING_PGTBL, my_vaddr, 0);
			rdtscll(e);

			if (unlikely(ret)) {
				printc("ACT failed on core %d>>>>>>>>>>>>>> %d, %d, %x\n", curr_cpu, ret, j, (unsigned int)my_vaddr);
				break;
			}

			curr_tick = printc("FLUSH!!");
			if (unlikely(curr_tick != last_tick)) {
//			if (last_tick+1 != curr_tick) printc("tick diff > 1: %u, %u\n", last_tick,curr_tick);
				last_tick = curr_tick;
				filter_out++;
				delay(KERN_QUIESCENCE_CYCLES);
			} else {
				diff = e-s;
				sum += diff;

				diff = (diff-avg);
				stddev_sum += (diff*diff);

				if (max < e-s) max = e-s;
			}
		}

		last_tick = printc("FLUSH!!");
		for (j = 0; j < 1024; j++) {
			my_vaddr = start_vaddr + j * PAGE_SIZE;
			s = tsc_start();
			ret1 = call_cap_op(PING_PGTBL, CAPTBL_OP_MEMDEACTIVATE,
					   my_vaddr, lid, 0, 0);
			rdtscll(e);

			if (unlikely(ret1)) {
				printc("mem DEACT failed on core %d>>>>>>>>>>>>>> %d, %d, %x\n", curr_cpu, ret1, j, (unsigned int)my_vaddr);
				break;
			}

			curr_tick = printc("FLUSH!!");
			if (unlikely(curr_tick != last_tick)) {
//			if (last_tick+1 != curr_tick) printc("tick diff > 1: %u, %u\n", last_tick,curr_tick);
				last_tick = curr_tick;
				filter_out++;
				delay(KERN_QUIESCENCE_CYCLES);
			} else {
				diff = e-s;
				sum_unmap += diff;

				diff = (diff-avg_unmap);
				stddev_sum_unmap += (diff*diff);

				if (max_unmap < e-s) max_unmap = e-s;
			}

		}

		tlb_tick = printc("FLUSH!!");
		while (1) {
			curr_tick = printc("FLUSH!!");
			if (curr_tick != last_tick) {
				delay(KERN_QUIESCENCE_CYCLES);
//				printc("curr tick %d, last %d\n", curr_tick, last_tick);

				break;
			}
		}
	}

	avg2 = sum / (ITER-filter_out);
	avg2_unmap = sum_unmap/(ITER-filter_out_b);
	stddev_sum /= (ITER-filter_out);
	stddev_sum_unmap /= (ITER-filter_out_b);
	if (avg != avg2) 
		printc(">>>>Warning: assumed average overhead of map not consistent with the measured number %llu, %llu\n", avg, avg2);
	if (avg_unmap != avg2_unmap) 
		printc(">>>>Warning: assumed average overhead of unmap not consistent with the measured number %llu, %llu\n", avg_unmap, avg2_unmap);


	printc("core %ld: @tick %u MEM map/unmap: avg %llu, max %llu, stddev^2 %llu; avg %llu, max %llu stddev^2 %llu. filtered %lu out of %d\n", cos_cpuid(), curr_tick, avg2, max, stddev_sum, 
	       avg2_unmap, max_unmap, stddev_sum_unmap, filter_out+filter_out_b, 2*ITER);

	ck_pr_store_int(&all_exit, 1);

	return;
}

void cos_init(void)
{
//	u64_t s, e;

	if (received[cos_cpuid()].snd_thd_created) {
		rcv_thd();
		BUG();
		return;
	}
	received[cos_cpuid()].snd_thd_created = 1;

	cap_switch_thd(RCV_THD_CAP_BASE + captbl_idsize(CAP_THD)*cos_cpuid());

	//init rcv thd first.

	/* if (cos_cpuid() == 0) { */
	/* 	pingpong(); */
	/* 	goto done; */
	/* } */

#if NUM_CPU > 2
//	else {	goto done; }
//	if (cos_cpuid() < (NUM_CPU_COS - SND_RCV_OFFSET)  && (cos_cpuid() % 4 == 0)) {
//	if ((cos_cpuid()%4 == 0 || cos_cpuid()%4 == 2) && (cos_cpuid()+SND_RCV_OFFSET < NUM_CPU_COS)) { // sending core
//	if (cos_cpuid() % 4 <= 1) {
	if (1) {
		/* IPI - ASND/ARCV */
//		ipi_test();
		cap_test();
//		mem_test();
	} else { //if ((cos_cpuid() % 4 <= 1)
//		printc("core %ld: thd %d switching to pong thd\n", cos_cpuid(), cos_get_thd_id());
		arcv_ready[cos_cpuid()] = 1;
		//printc("core %ld: doing operations as interference\n", cos_cpuid());
		////////////////////////
//		rdtscll(s);
		while (1) {
			//do op here to measure response time.
//			call_cap(4, 0, 0, 0, 0);
//			rdtscll(e);
//			if ((e-s)/(2000*1000*1000) > RUNTIME) break;

			if (ck_pr_load_int(&all_exit)) break;
		}
		printc("core %ld: exiting from ping\n", cos_cpuid());
	}
#endif
done:
	cap_switch_thd(SCHED_CAPTBL_ALPHATHD_BASE + cos_cpuid()*captbl_idsize(CAP_THD));

	call();

	return;
	/* u64_t start, end, avg, tot = 0, dev = 0; */
	/* int i, j; */


/* 	printc("cpu %ld, thd %d from ping\n",cos_cpuid(), cos_get_thd_id()); */
/* //	call(111,222,333,444);			/\* get stack *\/ */

/* 	printc("core %ld: spinning....\n", cos_cpuid()); */
/* 	delay(); */
/* 	printc("core %ld: after spin!\n", cos_cpuid()); */
	
/* //	call(1111,2222,3333,4444);			/\* get stack *\/ */
/* 	printc("Starting %d Invocations.\n", ITER); */
	
/* 	int params[8] = {1,2,3,4, 11, 22, 33, 44}; */
/* 	par_inv(test_fn, (void *)params, 2); */

/* 	return; */

/* 	for (i = 0 ; i < ITER ; i++) { */
/* 		rdtscll(start); */
/* //		cos_send_ipi(i, 0, 0, 0); */
/* 		call(1,2,3,4); */
/* 		rdtscll(end); */
/* 		meas[i] = end-start; */
/* 	} */

/* 	for (i = 0 ; i < ITER ; i++) tot += meas[i]; */
/* 	avg = tot/ITER; */
/* 	printc("avg %lld\n", avg); */
/* 	for (tot = 0, i = 0, j = 0 ; i < ITER ; i++) { */
/* 		if (meas[i] < avg*2) { */
/* 			tot += meas[i]; */
/* 			j++; */
/* 		} */
/* 	} */
/* 	printc("avg w/o %d outliers %lld\n", ITER-j, tot/j); */

/* 	for (i = 0 ; i < ITER ; i++) { */
/* 		u64_t diff = (meas[i] > avg) ?  */
/* 			meas[i] - avg :  */
/* 			avg - meas[i]; */
/* 		dev += (diff*diff); */
/* 	} */
/* 	dev /= ITER; */
/* 	printc("deviation^2 = %lld\n", dev); */

/* 	printc("last invocation...\n"); */
/* 	rdtscll(start); */
/* 	int rrr = call(11,22,33,44); */
/* 	rdtscll(end); */
/* 	printc("done ret %d. cost %llu \n", rrr, end-start); */

/* 	rdtscll(start); */
/* 	rrr = call(11,22,33,44); */
/* 	rdtscll(end); */
/* 	printc("done ret %d. cost %llu \n", rrr, end-start); */

/* //	printc("%d invocations took %lld\n", ITER, end-start); */
/* 	return; */
}
