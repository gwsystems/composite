#include <cos_component.h>
#include <cos_debug.h>

#include <pong.h>

#include <stdio.h>
#include <string.h>

#include "ops.h"

#define MEAS_MAP

int
prints(char *str)
{
	int len = strlen(str);
	cos_print(str, len);
	return len;
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
	cos_print(s, ret);

	return 0;
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

#define ITER (1024*1024) //(100*1000)

#include <ck_pr.h>
#define N_SYNC_CPU (NUM_CPU_COS)
int synced_nthd = 0;
void sync_all()
{
	int ret;
	if (N_SYNC_CPU <= 1) return;

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

	if (cos_cpuid() == 0) ck_pr_store_int(&all_exit, 1);

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

int start_flush, flush_done;

#define PING_MEM_START (0x44800000)

void response_test(void)
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

	if (cos_cpuid() == 0) {
		int j;
		avg = 3000;//6366;//3050;
//		while (ck_pr_load_int(&arcv_ready[target]) == 0) ;
//		printc("core %ld: start sending ipi\n", cos_cpuid());
		for (i = 0; i<ITER; i++) {
//			last = ck_pr_load_int(&curr_rcv->rcv);
			*pong_shmem = 0;
			j = i%1024;

			ck_pr_store_int(&start_flush, j + 1);
			while (ck_pr_load_int(&flush_done) == j) ;

			cos_inst_bar();
			last_tick = printc("FLUSH!!");
			cos_inst_bar();
			s1 = tsc_start();
			call_cap(ACAP_BASE + captbl_idsize(CAP_ASND)*target, 0, 0, 0, 0);
			/* rdtscll(e1); */
			/* sum2 += e1-s1; */

//			printc("cpu %d sent in ping\n", cos_cpuid());
			cos_inst_bar();

			e1 = 0;
			while (e1 == 0) { 
				/* rdtscll(e); */
				/* if (e - s1 > 5000000) { */
				/* 	printc("i %d....\n", i); */
				/* 	i = ITER + 1; */
				/* 	break; */
				/* } */
				e1 = *pong_shmem; 
			}
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
					printc("Warning SMI?>>>>>> cpu %ld curr_tick %d: %d\n", cos_cpuid(), curr_tick, diff);
					i--; 
					continue;
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

		printc("core %ld: ipi sending to %d avg %llu, max %llu, stddev^2 %llu\n", cos_cpuid(), target, sum/ITER, max, stddev / ITER);
		ck_pr_store_int(&all_exit, 1);
	} else if (1) { //(cos_cpuid() == SND_RCV_OFFSET || cos_cpuid() % 4 <= 1) {
		// doing work to interfere
		while (1) {
			unsigned long my_vaddr, start_vaddr = 0x80000000 - (1+cos_cpuid())*0x400000; /* no false sharing. */
			int curr_cpu = cos_cpuid();
			int j, ret, lid = 64 + 16*curr_cpu;

			for (j = 0; j < 1024; j++) {
				my_vaddr = start_vaddr + j * PAGE_SIZE;

				if (curr_cpu == SND_RCV_OFFSET) {
					while (ck_pr_load_int(&start_flush) == j) ;
					printc("FLUSH!!!__CACHE");
					ck_pr_store_int(&flush_done, j + 1);
				} else {
					while (ck_pr_load_int(&flush_done) == j) ;
				}

				ret = call_cap_op(PING_PGTBL, CAPTBL_OP_CPY,
						  PING_MEM_START, PING_PGTBL, my_vaddr, 0);

				if (unlikely(ret)) {
					printc("ACT failed on core %d>>>>>>>>>>>>>> %d, %d, %x\n", curr_cpu, ret, j, (unsigned int)my_vaddr);
					break;
				}

			}

			for (j = 0; j < 1024; j++) {
				my_vaddr = start_vaddr + j * PAGE_SIZE;
				ret = call_cap_op(PING_PGTBL, CAPTBL_OP_MEMDEACTIVATE,
						   my_vaddr, lid, 0, 0);
				if (unlikely(ret)) {
					printc("mem DEACT failed on core %d>>>>>>>>>>>>>> %d, %d, %x\n", curr_cpu, ret, j, (unsigned int)my_vaddr);
					break;
				}
			}

			last_tick = printc("FLUSH!!");
			while (1) {
				curr_tick = printc("FLUSH!!");
				if (curr_tick != last_tick) {
					delay(KERN_QUIESCENCE_CYCLES);

					break;
				}
			}

			if (ck_pr_load_int(&all_exit)) break;
		}
		printc("core %ld doing response_test done\n", cos_cpuid());
	} else {
		while (1) {
			if (ck_pr_load_int(&all_exit)) break;
		}
	}

	goto done;
done:
	return;
}

void cap_test(void)
{
	int i, curr_cpu, my_cap, ret, ret1, n_valid = 0;
	long long diff;
	u64_t s, e, avg, avg2 = 0;
	u64_t sum = 0, max = 0, stddev_sum = 0;
	volatile u32_t last_tick, curr_tick;
	int lid;

//#ifdef MEAS_AVG
	curr_cpu = cos_cpuid();

#define CAPTBL_LEAFSZ  16

#define FALSE_SHARING
#ifdef FALSE_SHARING
	my_cap = PAGE_SIZE/2/CAPTBL_LEAFSZ*510;
#else	
	/* no false sharing. different pages to avoid prefetching */
	my_cap = PAGE_SIZE/2/CAPTBL_LEAFSZ*510 - curr_cpu*PAGE_SIZE/CAPTBL_LEAFSZ;
#endif
	lid = 64 + 16*curr_cpu;

	printc("core %d: doing cap map/unmap test @ cap %d, lid %d\n", curr_cpu, my_cap, lid);
	
	avg = 800; /* on the 40-core server.*/
	//avg = 404; /* on the 40-core server.*/

	sum = 0;
	/////////////////
	last_tick = printc("FLUSH!!");
	/* printc("core %ld: doing pingpong w/ flush @tick %u!\n", cos_cpuid(), last_tick); */
	for (i = 0; i < ITER; i++) {
		last_tick = printc("FLUSH!!");
		s = tsc_start();
		ret = call_cap_op(PING_CAPTBL, CAPTBL_OP_SINVACTIVATE,
				  my_cap, PING_COMPCAP, 123, 0);
		rdtscll(e);
		///////////////

//		s = tsc_start();
		ret1 = call_cap_op(PING_CAPTBL, CAPTBL_OP_SINVDEACTIVATE,
				  my_cap, lid, 0, 0);
//		rdtscll(e);

//		if (e-s > 20000) printc("large: %llu @ %llu\n", e-s, e);
		delay(KERN_QUIESCENCE_CYCLES);

#ifndef FALSE_SHARING
		if (unlikely(ret || ret1)) printc("ACT/DEACT failed on core %d>>>>>>>>>>>>>> %d %d\n", curr_cpu, ret, ret1);
#endif

		curr_tick = printc("FLUSH!!");
		if (unlikely(curr_tick != last_tick)) {
//			printc("timer detected @ %llu, %d, %d, (cost %llu)\n", e, last_tick, curr_tick, e-s);
//			if (last_tick+1 != curr_tick) printc("tick diff > 1: %u, %u\n", last_tick,curr_tick);
			last_tick = curr_tick;
			i--;
			continue;
		}
		if (ret == 0) {
			n_valid++;
			diff = e-s;
			sum += diff;

			diff = (diff-avg);
			stddev_sum += (diff*diff);

			if (max < e-s) max = e-s;
		}
	}
	if (n_valid) {
		avg2 = sum /n_valid;
		stddev_sum /= n_valid;
	} else 
		printc("cpu %d has n_valid 0!!!\n", curr_cpu);
	if (avg != avg2) 
		printc(">>>>Warning: assumed average overhead not consistent with the measured number %llu, %llu\n", avg, avg2);

	printc("core %ld: @tick %u cap map/unmap done, avg %llu, max %llu, stddev^2 %llu\n", 
	       cos_cpuid(), curr_tick, avg2, max, stddev_sum);

	if (cos_cpuid() == 0) ck_pr_store_int(&all_exit, 1);

	return;
}

void mem_test(void)
{
	int n_loops, ii, j, curr_cpu, ret, ret1, n_valid = 0, n_valid_2 = 0;
	unsigned long my_vaddr, start_vaddr, filter_out = 0, filter_out_b = 0;
	long long diff;
	u64_t s, e, avg, avg_unmap, avg2 = 0, avg2_unmap = 0;
	u64_t sum = 0, max = 0, stddev_sum = 0, sum_unmap = 0, stddev_sum_unmap = 0, max_unmap = 0;
	volatile u32_t last_tick, curr_tick, tlb_tick;
	int lid;

	curr_cpu = cos_cpuid();

//#define FALSE_SHARING
#ifdef FALSE_SHARING
	start_vaddr = 0x80000000 - 0x400000; /* NOTE: WITH FALSE SHARING! */
#else
	start_vaddr = 0x80000000 - (1+cos_cpuid())*0x400000; /* no false sharing. */
#endif
	lid = 64 + 16*curr_cpu;

//	printc("core %d: doing MEM map/unmap test\n", curr_cpu);
	
	avg = 2749;//1360; /* on the 40-core server.*/
	avg_unmap = 3248; 

	sum = 0;
	/////////////////
	/* printc("core %ld: doing pingpong w/ flush @tick %u!\n", cos_cpuid(), last_tick); */
	curr_tick = last_tick = printc("FLUSH!!");

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

#ifndef FALSE_SHARING
			if (unlikely(ret)) {
				printc("ACT failed on core %d>>>>>>>>>>>>>> %d, %d, %x\n", curr_cpu, ret, j, (unsigned int)my_vaddr);
				break;
			}
#endif
			curr_tick = printc("FLUSH!!");
			if (unlikely(curr_tick != last_tick)) {
//			if (last_tick+1 != curr_tick) printc("tick diff > 1: %u, %u\n", last_tick,curr_tick);
				last_tick = curr_tick;
				filter_out++;
				delay(KERN_QUIESCENCE_CYCLES);
			}
			
			if (ret == 0) {
				n_valid++;

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
#ifndef FALSE_SHARING
			if (unlikely(ret1)) {
				printc("mem DEACT failed on core %d>>>>>>>>>>>>>> %d, %d, %x\n", curr_cpu, ret1, j, (unsigned int)my_vaddr);
				break;
			}
#endif
			curr_tick = printc("FLUSH!!");
			if (unlikely(curr_tick != last_tick)) {
//			if (last_tick+1 != curr_tick) printc("tick diff > 1: %u, %u\n", last_tick,curr_tick);
				last_tick = curr_tick;
				filter_out_b++;
				delay(KERN_QUIESCENCE_CYCLES);
			} 
			
			if (ret1 == 0) {
				n_valid_2++;
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

	if (n_valid) {
		avg2 = sum / (n_valid);
		stddev_sum /= (n_valid);
	} else {
		printc("cpu %d n_valid 0\n", curr_cpu);
	}


	if (n_valid_2) {
		avg2_unmap = sum_unmap/(n_valid_2);
		stddev_sum_unmap /= (n_valid_2);
	} else {
		printc("cpu %d n_valid_2 0\n", curr_cpu);
	}

	if (avg != avg2) 
		printc(">>>>Warning: assumed average overhead of map not consistent with the measured number %llu, %llu\n", avg, avg2);
	if (avg_unmap != avg2_unmap) 
		printc(">>>>Warning: assumed average overhead of unmap not consistent with the measured number %llu, %llu\n", avg_unmap, avg2_unmap);


	if (filter_out+filter_out_b > 10) printc("cpu %ld filtered %lu out of %d\n", cos_cpuid(), filter_out+filter_out_b, 2*ITER);

	printc("core %ld: @tick %u MEM map/unmap: avg %llu, max %llu, stddev^2 %llu; avg %llu, max %llu stddev^2 %llu\n", 
	       cos_cpuid(), curr_tick, avg2, max, stddev_sum, avg2_unmap, max_unmap, stddev_sum_unmap);

	if (cos_cpuid() == 0) ck_pr_store_int(&all_exit, 1);

	return;
}

void cons_decons_test(void)
{
	int i, curr_cpu, ret, ret1;
	long long diff;
	u64_t s, e, avg, avg_decons, avg2, avg2_decons;
	u64_t sum = 0, max = 0, stddev_sum = 0, sum_decons = 0, stddev_sum_decons = 0, max_decons = 0;
	volatile u32_t last_tick, curr_tick;

//#ifdef MEAS_AVG
	curr_cpu = cos_cpuid();

//	printc("core %d: doing MEM map/decons test\n", curr_cpu);
	
	//avg = 516;//1360; /* on the 40-core server.*/
	avg = 337;//1360; /* on the 40-core server.*/
	avg_decons = 328; 

	sum = 0;
	/////////////////
	/* printc("core %ld: doing pingpong w/ flush @tick %u!\n", cos_cpuid(), last_tick); */
	curr_tick = last_tick = printc("FLUSH!!");

	last_tick = printc("FLUSH!!");
	for (i = 0; i < ITER; i++) {
		s = tsc_start();
#define CONS_TEST_ADDR (PING_MEM_START + 0x4000000)
		ret = call_cap_op(PING_PGTBL, CAPTBL_OP_CONS,
				  PING_PGTBL2, CONS_TEST_ADDR, 0, 0);
		rdtscll(e);

		/* if (unlikely(ret)) { */
		/* 	printc("CONS failed on core %d>>>>>>>>>>>>>> %d, %d\n", curr_cpu, ret, i); */
		/* 	break; */
		/* } else printc("CONS ret %d\n", ret); */

		curr_tick = printc("FLUSH!!");
		if (unlikely(curr_tick != last_tick)) {
//			if (last_tick+1 != curr_tick) printc("tick diff > 1: %u, %u\n", last_tick,curr_tick);
			ret1 = call_cap_op(PING_PGTBL, CAPTBL_OP_DECONS,
					   PING_PGTBL2, CONS_TEST_ADDR, 1, 0);
			last_tick = curr_tick;
			delay(KERN_QUIESCENCE_CYCLES);
			i--;
			continue;
		} else {
			diff = e-s;
			sum += diff;

			diff = (diff-avg);
			stddev_sum += (diff*diff);

			if (max < e-s) max = e-s;
		}

		s = tsc_start();
		ret1 = call_cap_op(PING_PGTBL, CAPTBL_OP_DECONS,
				   PING_PGTBL2, CONS_TEST_ADDR, 1, 0);
		rdtscll(e);

		/* if (unlikely(ret1)) { */
		/* 	printc("DECONS failed on core %d>>>>>>>>>>>>>> %d, %d\n", curr_cpu, ret1, i); */
		/* 	break; */
		/* } else printc("DECONS ret %d\n", ret); */

		curr_tick = printc("FLUSH!!");
		if (unlikely(curr_tick != last_tick)) {
//			if (last_tick+1 != curr_tick) printc("tick diff > 1: %u, %u\n", last_tick,curr_tick);
			last_tick = curr_tick;
			i--;
			continue;
		} else {
			diff = e-s;
			sum_decons += diff;

			diff = (diff-avg_decons);
			stddev_sum_decons += (diff*diff);

			if (max_decons < e-s) max_decons = e-s;
		}

	}

	avg2 = sum / (ITER);
	avg2_decons = sum_decons/(ITER);
	stddev_sum /= (ITER);
	stddev_sum_decons /= (ITER);
	if (avg != avg2) 
		printc(">>>>Warning: assumed average overhead of cons not consistent with the measured number %llu, %llu\n", avg, avg2);
	if (avg_decons != avg2_decons) 
		printc(">>>>Warning: assumed average overhead of decons not consistent with the measured number %llu, %llu\n", avg_decons, avg2_decons);


	printc("core %ld: @tick %u pgtbl cons/decons: avg %llu, max %llu, stddev^2 %llu; avg %llu, max %llu stddev^2 %llu. \n", cos_cpuid(), curr_tick, avg2, max, stddev_sum, 
	       avg2_decons, max_decons, stddev_sum_decons);

	if (cos_cpuid() == 0) ck_pr_store_int(&all_exit, 1);

	return;
}

void captbl_cons_test(void)
{
	int i, curr_cpu, ret, ret1;
	long long diff;
	u64_t s, e, avg, avg_decons, avg2, avg2_decons;
	u64_t sum = 0, max = 0, stddev_sum = 0, sum_decons = 0, stddev_sum_decons = 0, max_decons = 0;
	volatile u32_t last_tick, curr_tick;

//#ifdef MEAS_AVG
	curr_cpu = cos_cpuid();

//	printc("core %d: doing MEM map/decons test\n", curr_cpu);
	
	//avg = 516;//1360; /* on the 40-core server.*/
	avg = 62518;//1360; /* on the 40-core server.*/
	avg_decons = 48878; 

	sum = 0;
	/////////////////
	/* printc("core %ld: doing pingpong w/ flush @tick %u!\n", cos_cpuid(), last_tick); */
	curr_tick = last_tick = printc("FLUSH!!");

	last_tick = printc("FLUSH!!");
	for (i = 0; i < ITER; i++) {
		s = tsc_start();
#define CONS_TEST_CAP (PAGE_SIZE/2/CAPTBL_LEAFSZ*100)
		ret = call_cap_op(PING_CAPTBL, CAPTBL_OP_CONS,
				  PING_CAPTBL2, CONS_TEST_CAP, 0, 0);
		rdtscll(e);

		/* if (unlikely(ret)) { */
		/* 	printc("CONS failed on core %d>>>>>>>>>>>>>> %d, %d\n", curr_cpu, ret, i); */
		/* 	break; */
		/* } //else printc("CONS ret %d\n", ret); */

		curr_tick = printc("FLUSH!!");
		if (unlikely(curr_tick != last_tick)) {
//			if (last_tick+1 != curr_tick) printc("tick diff > 1: %u, %u\n", last_tick,curr_tick);
			ret1 = call_cap_op(PING_CAPTBL, CAPTBL_OP_DECONS,
					   PING_CAPTBL2, CONS_TEST_CAP, 1, 0);

			last_tick = curr_tick;
			delay(KERN_QUIESCENCE_CYCLES);
			i--;
			continue;
		} else {
			diff = e-s;
			sum += diff;

			diff = (diff-avg);
			stddev_sum += (diff*diff);

			if (max < e-s) max = e-s;
		}

		s = tsc_start();
		ret1 = call_cap_op(PING_CAPTBL, CAPTBL_OP_DECONS,
				   PING_CAPTBL2, CONS_TEST_CAP, 1, 0);
		rdtscll(e);

		/* if (unlikely(ret1)) { */
		/* 	printc("DECONS failed on core %d>>>>>>>>>>>>>> %d, %d\n", curr_cpu, ret1, i); */
		/* 	break; */
		/* } //else printc("DECONS ret %d\n", ret1); */

		curr_tick = printc("FLUSH!!");
		if (unlikely(curr_tick != last_tick)) {
//			if (last_tick+1 != curr_tick) printc("tick diff > 1: %u, %u\n", last_tick,curr_tick);
			last_tick = curr_tick;
			i--;
			continue;
		} else {
			diff = e-s;
			sum_decons += diff;

			diff = (diff-avg_decons);
			stddev_sum_decons += (diff*diff);

			if (max_decons < e-s) max_decons = e-s;
		}

	}

	avg2 = sum / (ITER);
	avg2_decons = sum_decons/(ITER);
	stddev_sum /= (ITER);
	stddev_sum_decons /= (ITER);
	if (avg != avg2) 
		printc(">>>>Warning: assumed average overhead of cons not consistent with the measured number %llu, %llu\n", avg, avg2);
	if (avg_decons != avg2_decons) 
		printc(">>>>Warning: assumed average overhead of decons not consistent with the measured number %llu, %llu\n", avg_decons, avg2_decons);


	printc("core %ld: @tick %u pgtbl cons/decons: avg %llu, max %llu, stddev^2 %llu; avg %llu, max %llu stddev^2 %llu. \n", cos_cpuid(), curr_tick, avg2, max, stddev_sum, 
	       avg2_decons, max_decons, stddev_sum_decons);

	if (cos_cpuid() == 0) ck_pr_store_int(&all_exit, 1);

	return;
}

void retype_test(void)
{
	int i, curr_cpu, ret, ret1, n_valid = 0, n_valid_2 = 0;
	long long diff;
	u64_t s, e, avg_retype2user, avg_2frame, avg2_retype2user=0, avg2_2frame=0;
	u64_t sum = 0, max = 0, stddev_sum = 0, sum_2frame = 0, stddev_sum_2frame = 0, max_2frame = 0;
	volatile u32_t last_tick, curr_tick;

//#ifdef MEAS_AVG
	curr_cpu = cos_cpuid();

//	printc("core %d: doing MEM map/2frame test\n", curr_cpu);
	
	//avg = 516;//1360; /* on the 40-core server.*/
	avg_retype2user = 18753;//1360; /* on the 40-core server.*/
	avg_2frame = 23546; 

	sum = 0;
	/////////////////
	/* printc("core %ld: doing pingpong w/ flush @tick %u!\n", cos_cpuid(), last_tick); */
	curr_tick = last_tick = printc("FLUSH!!");

	last_tick = printc("FLUSH!!");
	for (i = 0; i < ITER; i++) {
		s = tsc_start();
#define RETYPE_ADDR (BOOT_MEM_KM_BASE + COS_MAX_MEMORY*PAGE_SIZE - RETYPE_MEM_SIZE)
		ret = call_cap_op(PING_ROOTPGTBL, CAPTBL_OP_MEM_RETYPE2USER,
				  RETYPE_ADDR, 0, 0, 0);
		rdtscll(e);

		/* if (unlikely(ret)) { */
		/* 	printc("retype2user failed on core %d>>>>>>>>>>>>>> %d, %d\n", curr_cpu, ret, i); */
		/* 	break; */
		/* } */

		curr_tick = printc("FLUSH!!");
		if (unlikely(curr_tick != last_tick)) {
//			if (last_tick+1 != curr_tick) printc("tick diff > 1: %u, %u\n", last_tick,curr_tick);
			ret = call_cap_op(PING_ROOTPGTBL, CAPTBL_OP_MEM_RETYPE2FRAME,
					  RETYPE_ADDR, 0, 0, 0);

			last_tick = curr_tick;
			delay(KERN_QUIESCENCE_CYCLES);
			i--;
			continue;
		} 
		if (ret == 0) {
			n_valid++;
			diff = e-s;
			sum += diff;

			diff = (diff-avg_retype2user);
			stddev_sum += (diff*diff);

			if (max < e-s) max = e-s;
		}

		s = tsc_start();
		ret1 = call_cap_op(PING_ROOTPGTBL, CAPTBL_OP_MEM_RETYPE2FRAME,
				   RETYPE_ADDR, 0, 0, 0);
		rdtscll(e);

		/* if (unlikely(ret1)) { */
		/* 	printc("Retype2Frame failed on core %d>>>>>>>>>>>>>> %d, %d\n", curr_cpu, ret1, i); */
		/* 	break; */
		/* } */

		curr_tick = printc("FLUSH!!");
		if (unlikely(curr_tick != last_tick)) {
//			if (last_tick+1 != curr_tick) printc("tick diff > 1: %u, %u\n", last_tick,curr_tick);
			last_tick = curr_tick;
			i--;
			continue;
		} 
		if (ret1 == 0) {
			n_valid_2++;
			diff = e-s;
			sum_2frame += diff;

			diff = (diff-avg_2frame);
			stddev_sum_2frame += (diff*diff);

			if (max_2frame < e-s) max_2frame = e-s;
		}

	}

	if (n_valid) {
		avg2_retype2user = sum / (n_valid);
		stddev_sum /= (n_valid);
	} else {
		printc("cpu %d n_valid 0\n", curr_cpu);
	}

	if (n_valid_2) {
		avg2_2frame = sum_2frame/(n_valid_2);
		stddev_sum_2frame /= (n_valid_2);
	} else {
		printc("cpu %d n_valid_2 0\n", curr_cpu);
	}

	if (avg_retype2user != avg2_retype2user) 
		printc(">>>>Warning: assumed average overhead of cons not consistent with the measured number %llu, %llu\n", avg_retype2user, avg2_retype2user);
	if (avg_2frame != avg2_2frame) 
		printc(">>>>Warning: assumed average overhead of 2frame not consistent with the measured number %llu, %llu\n", avg_2frame, avg2_2frame);


	printc("cpu%ld: @t %u retype: avg %llu, max %llu, sd %llu; avg %llu, max %llu sd %llu. \n", cos_cpuid(), curr_tick, avg2_retype2user, max, stddev_sum, 
	       avg2_2frame, max_2frame, stddev_sum_2frame);

	if (cos_cpuid() == 0) ck_pr_store_int(&all_exit, 1);

	return;
}

void kobj_test(void)
{
	int i, curr_cpu, ret, ret1;
	long long diff;
	u64_t s, e, avg_kobjact, avg_kobjdeact, avg2_kobjact = 0 , avg2_kobjdeact = 0, n_iter = 0, n_iter_b = 0;
	u64_t sum = 0, max = 0, stddev_sum = 0, sum_kobjdeact = 0, stddev_sum_kobjdeact = 0, max_kobjdeact = 0;
	volatile u32_t last_tick, curr_tick;
	int lid = 64 + cos_cpuid();

#define KOBJ_ID  (PING_CAP_FREE)
#define KMEM_ID  (BOOT_MEM_KM_BASE + 600*PAGE_SIZE)

	curr_cpu = cos_cpuid();

	int my_kobj_cap = KOBJ_ID + curr_cpu*CAP32B_IDSZ;
	int my_kmem = KMEM_ID + curr_cpu*PAGE_SIZE;

//#ifdef MEAS_AVG

//	printc("core %d: doing MEM map/kobjdeact test\n", curr_cpu);
	
	//avg = 516;//1360; /* on the 40-core server.*/
	avg_kobjact = 2426;//1360; /* on the 40-core server.*/
	avg_kobjdeact = 4501; 

	sum = 0;
	/////////////////
	/* printc("core %ld: doing pingpong w/ flush @tick %u!\n", cos_cpuid(), last_tick); */
	curr_tick = last_tick = printc("FLUSH!!");

	last_tick = printc("FLUSH!!");
	for (i = 0; i < ITER; i++) {
		s = tsc_start();
		ret = call_cap_op(PING_CAPTBL, CAPTBL_OP_CAPTBLACTIVATE,
				  my_kobj_cap, PING_ROOTPGTBL, my_kmem, 1);
		rdtscll(e);

		/* if (curr_cpu == 0 && unlikely(ret)) { */
		/* 	printc("kobjact failed on core %d>>>>>>>>>>>>>> %d, %d\n", curr_cpu, ret, i); */
		/* } */

		curr_tick = printc("FLUSH!!");
		if (unlikely(curr_tick != last_tick)) {
//			if (last_tick+1 != curr_tick) printc("tick diff > 1: %u, %u\n", last_tick,curr_tick);

			last_tick = curr_tick;
			delay(KERN_QUIESCENCE_CYCLES);
			i--;
			continue;
		} 

		if (ret == 0) {
			n_iter++;
			diff = e-s;
			sum += diff;

			diff = (diff-avg_kobjact);
			stddev_sum += (diff*diff);

			if (max < e-s) max = e-s;
		}

//		s = tsc_start();
		ret1 = call_cap_op(PING_CAPTBL, CAPTBL_OP_CAPKMEM_FREEZE, 
				  my_kobj_cap, 0, 0, 0);
//		rdtscll(e);
		delay(KERN_QUIESCENCE_CYCLES);

		/* if (curr_cpu == 0 && unlikely(ret1)) { */
		/* 	printc("kobj freeze failed on core %d>>>>>>>>>>>>>> %d, %d\n", curr_cpu, ret1, i); */
		/* } */

		s = tsc_start();
		ret1 = call_cap_op(PING_CAPTBL, CAPTBL_OP_CAPTBLDEACTIVATE_ROOT, 
				   my_kobj_cap, lid, PING_ROOTPGTBL, my_kmem);
		rdtscll(e);

		delay(KERN_QUIESCENCE_CYCLES);
		curr_tick = printc("FLUSH!!");
		if (unlikely(curr_tick != last_tick)) {
//			if (last_tick+1 != curr_tick) printc("tick diff > 1: %u, %u\n", last_tick,curr_tick);
			last_tick = curr_tick;
			i--;
			continue;
		} 

		if (ret1 == 0) {
			n_iter_b++;
			diff = e-s;
			sum_kobjdeact += diff;

			diff = (diff-avg_kobjdeact);
			stddev_sum_kobjdeact += (diff*diff);

			if (max_kobjdeact < e-s) max_kobjdeact = e-s;
		}
	}

	if (n_iter) {
		avg2_kobjact = sum / (n_iter);
		stddev_sum /= (n_iter);
	} else {
		printc("core %ld: ITER is 0!!!!\n", cos_cpuid());
	}

	if (n_iter_b) {
		avg2_kobjdeact = sum_kobjdeact/(n_iter_b);
		stddev_sum_kobjdeact /= (n_iter_b);
	} else {
		printc("core %ld: ITER_b is 0!!!!\n", cos_cpuid());
	}

	if (avg_kobjact != avg2_kobjact) 
		printc(">>>>Warning: assumed average overhead of kobjact not consistent with the measured number %llu, %llu\n", avg_kobjact, avg2_kobjact);
	if (avg_kobjdeact != avg2_kobjdeact) 
		printc(">>>>Warning: assumed average overhead of kobjdeact not consistent with the measured number %llu, %llu\n", avg_kobjdeact, avg2_kobjdeact);


	printc("%ld: @tick %u kobjact/deact: i %llu avg %llu, max %llu sd %llu; i %llu avg %llu, max %llu sd %llu \n", cos_cpuid(), curr_tick, n_iter, avg2_kobjact, max, stddev_sum, 
	       n_iter_b, avg2_kobjdeact, max_kobjdeact, stddev_sum_kobjdeact);

	if (cos_cpuid() == 0) ck_pr_store_int(&all_exit, 1);

	return;
}

void thd_test(void)
{
	int i, curr_cpu, my_cap, ret, ret1;
	long long diff;
	u64_t s, e, avg, avg2;
	u64_t sum = 0, max = 0, stddev_sum = 0;
	volatile u32_t last_tick, curr_tick;
	volatile u64_t *pong_shmem;

	curr_cpu = cos_cpuid();
	pong_shmem = (u64_t *)&shmem[(curr_cpu) * CACHE_LINE];
	my_cap = RCV_THD_CAP_BASE + captbl_idsize(CAP_THD)*cos_cpuid();

	printc("core %d: doing thd switch test @ cap %d\n", curr_cpu, my_cap);
	
	avg = 463; /* on the 40-core server.*/

	sum = 0;

	/////////////////
	last_tick = printc("FLUSH!!");
	/* printc("core %ld: doing pingpong w/ flush @tick %u!\n", cos_cpuid(), last_tick); */
	for (i = 0; i < ITER; i++) {
//		printc("PING: core %d switching to thd %d\n", cos_cpuid(), my_cap);

		s = tsc_start();
		ret = cap_switch_thd(my_cap);
		e = *pong_shmem;

		if (unlikely(ret)) printc("ACT/DEACT failed on core %d>>>>>>>>>>>>>> %d\n", curr_cpu, ret);

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

	printc("core %ld: @tick %u thd_switch done, avg %llu, max %llu, stddev^2 %llu\n", 
	       cos_cpuid(), curr_tick, avg2, max, stddev_sum);

	if (cos_cpuid() == 0) ck_pr_store_int(&all_exit, 1);

	return;
}

void wcet_test(void)
{
	int i, curr_cpu, my_cap, ret = 0, ret1;
	long long diff;
	u64_t s, e, avg, avg2;
	u64_t sum = 0, max = 0, stddev_sum = 0;
	volatile u32_t last_tick, curr_tick;
	volatile u64_t *pong_shmem;

	curr_cpu = cos_cpuid();
	pong_shmem = (u64_t *)&shmem[(curr_cpu) * CACHE_LINE];
	my_cap = RCV_THD_CAP_BASE + captbl_idsize(CAP_THD)*cos_cpuid();

	printc("core %d: doing thd switch test @ cap %d\n", curr_cpu, my_cap);
	
	avg = 463; /* on the 40-core server.*/

	sum = 0;

	/////////////////
	last_tick = printc("FLUSH!!");
	/* printc("core %ld: doing pingpong w/ flush @tick %u!\n", cos_cpuid(), last_tick); */
	for (i = 0; i < ITER; i++) {
//		printc("PING: core %d switching to thd %d\n", cos_cpuid(), my_cap);

		s = tsc_start();

		/* ret = cap_switch_thd(my_cap); */
		/* e = *pong_shmem; */

		call_cap(4, 0, 0, 0, 0);
		rdtscll(e);

		if (unlikely(ret)) printc("ACT/DEACT failed on core %d>>>>>>>>>>>>>> %d\n", curr_cpu, ret);

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

	printc("core %ld: @tick %u thd_switch done, avg %llu, max %llu, stddev^2 %llu\n", 
	       cos_cpuid(), curr_tick, avg2, max, stddev_sum);

	if (cos_cpuid() == 0) ck_pr_store_int(&all_exit, 1);

	return;
}

#include <mem_mgr.h>
/* FIEME: transparent invocation linking! */
#define MMAN_VALLOC 6
#define MMAN_GET    8
#define MMAN_ALIAS  10
#define MMAN_REVOKE 12
#define MMAN_RELEASE 14

void
memmgr_unit_test(void)
{
	int i, ret; 
#define N_OPS 16
	vaddr_t vas[N_OPS], mem[N_OPS];
	
	for (i = 0; i < N_OPS; i++) {
//		printc("cpu %d: i %d\n", cos_cpuid(), i);
		ret = call_cap(MMAN_VALLOC, cos_spd_id(), cos_spd_id(), 1, 0);
		vas[i] = (vaddr_t)ret;
		if (!ret) {
			printc("cpu %ld: i %d>>> comp %ld called valloc cap %d, ret %x\n", 
			       cos_cpuid(), i, cos_spd_id(), MMAN_VALLOC, ret);
			goto done;
		}
	}
	for (i = 0; i < N_OPS; i++) {
		ret = call_cap(MMAN_GET, cos_spd_id(), 0, 1 << 16, 0);
		mem[i] = (vaddr_t)ret;
		if (!ret) { printc("%d >>> comp %ld called mman_get cap %d, ret %x\n", i, cos_spd_id(), MMAN_GET, ret); goto done;}
	}

	for (i = 0; i < N_OPS; i++) {
		ret = call_cap(MMAN_ALIAS, cos_spd_id(), mem[i], cos_spd_id()<<16, vas[i]);
		if (!ret) { printc("comp %ld called alias cap %d, ret %x\n", cos_spd_id(), MMAN_ALIAS, ret); goto done;}
	}
	/* alias validation */
	for (i = 0; i < N_OPS; i++) {
		int *test = (int*)(vas[i]);
		*test = i;
	}
	for (i = 0; i < N_OPS; i++) {
		int *test = (int*)(mem[i]);
		if (*test != i) {
			printc("test failded! %d: %d @ %p\n", i, *test, test); 
			goto done;
		}
	}
	printc("CPU %ld: VALLOC, GET_PAGE, ALIAS test done!\n", cos_cpuid());

	for (i = 0; i < N_OPS; i++) {
		ret = call_cap(MMAN_REVOKE, cos_spd_id(), vas[i], 0, 0);
		if (ret) { printc("comp %ld called revoke cap %d, ret %x\n", cos_spd_id(), MMAN_REVOKE, ret); goto done; }
	}
	for (i = 0; i < N_OPS; i++) {
		ret = call_cap(MMAN_REVOKE, cos_spd_id(), mem[i], 0, 0);
		if (ret) { printc("comp %ld called revoke cap %d, ret %x\n", cos_spd_id(), MMAN_REVOKE, ret); goto done; }
	}
	for (i = 0; i < N_OPS; i++) {
		ret = call_cap(MMAN_RELEASE, cos_spd_id(), mem[i], 0, 0);
		if (ret) { printc("comp %ld called release cap %d, ret %x\n", cos_spd_id(), MMAN_RELEASE, ret); goto done; }
	}
	printc("CPU %ld: MM_REVOKE / _RELEASE test done!\n", cos_cpuid());

	ret = call_cap(MMAN_GET, cos_spd_id(), 0, 256 << 16, 0);
	mem[0] = (vaddr_t)ret;
	if (!ret) {
		printc("multipage >>> comp %ld called mman_get cap %d, ret %x\n", cos_spd_id(), MMAN_GET, ret); 
		goto done;
	}
	memset((void *)ret, 0, (256)*PAGE_SIZE);
	ret = call_cap(MMAN_RELEASE, cos_spd_id(), mem[0], 0, 0);
	if (ret) { printc("comp %ld release 1MB ret %x\n", cos_spd_id(), ret); goto done; }

	printc("CPU %ld: Multi-page allocation / free tests done!\n", cos_cpuid());

//	if (cos_cpuid() == 0) {
	{
		int tot_mb = 0;
		while (1) {
			ret = call_cap(MMAN_GET, cos_spd_id(), 0, 256 << 16, 0);
//			printc("%d: got a 1mb page @ %x!\n", tot_mb, ret);
			if (ret == 0) break;
			memset((void *)ret, 0, (256)*PAGE_SIZE);
			tot_mb += 1;
		}
		printc("CPU %ld: MM allocation test: %d MBs allocated in total\n", cos_cpuid(), tot_mb);
	}
#undef N_OPS
done:
	return;
}

#define N_OPS 1024
unsigned long allmem[NUM_CPU][N_OPS+16];

int mm_meas(void)
{
	int i, cpu, ret;
	cpu = cos_cpuid();

	for (i = 0; i < N_OPS; i++) {
		ret = call_cap(MMAN_GET, cos_spd_id(), 0, 1 << 16, 0);
		allmem[cpu][i] = ret;
		if (!ret) { 
			printc("cpu %d, %d >>> comp %ld called mman_get cap %d, ret %x\n", cpu, i, cos_spd_id(), MMAN_GET, ret); 
			ret = -1;
			goto done;
		}
//		memset((void *)ret, 0, PAGE_SIZE);
	}
	for (i = 0; i < N_OPS; i++) {
		ret = call_cap(MMAN_RELEASE, cos_spd_id(), allmem[cpu][i], 0, 0);
		if (ret) { 
			printc("comp %ld called release cap %d, ret %x\n", cos_spd_id(), MMAN_RELEASE, ret); 
			ret = -1;
			goto done; 
		}
	}
	ret = 0;
done:
	return ret;
}

void mm_stress(void)
{
	unsigned long long s, e, tot = 0;
	int i, cpu = cos_cpuid();

	for (i = 0; i < 1024; i++) {
		s = tsc_start();
		if (mm_meas()) {
			printc("cpu %d failed when iter %d\n", cpu, i);
			goto done;
		}
		e = tsc_start();
		tot += e-s;
	}
	printc("cpu %d, avg cost %llu\n", cpu, (tot)/N_OPS/1024);
done:
	return;
}

//only record core 0
unsigned long logs[10000], nlog = 0;
unsigned long logs_large[1000], nlog_large = 0;

void play_trace(unsigned long *npage_alloc, unsigned long long *tot, unsigned long *npage_1024, unsigned long long *tot_1024)
{
	int i, ret, cpu = cos_cpuid(); 
	unsigned long long s,e;

	int npages, j, nmaps = 0;

	for (i = 0; i < NOPS; i++) {
		allmem[cpu][i] = 0;
		if (ops[cpu][i] == 1) {
			npages = 1;
		} else if (ops[cpu][i] == 2) {
			npages = 1024;
		} else 
			break;
		s = tsc_start();
		ret = call_cap(MMAN_GET, cos_spd_id(), 0, npages << 16, 0);
		if (!ret) {
			printc("cpu %d, i %d >>> comp %ld called mman_get cap %d, ret %x, allocated %lu pages already.\n", 
			       cpu, i, cos_spd_id(), MMAN_GET, ret, *npage_alloc); 
			ret = -1;
			goto done;
		}
		e = tsc_start();
#ifdef MEAS_MAP
		if (ops[cpu][i] == 1) {
			*tot = *tot + (e-s);
			if (cpu == 0) logs[nlog++] = (unsigned long)(e-s);
		} else {
			*tot_1024 = *tot_1024 + (e-s);
			if (cpu == 0) logs_large[nlog_large++] = (unsigned long)(e-s);
		}
#endif
		allmem[cpu][i] = ret;
		memset((void *)ret, 0, PAGE_SIZE*npages);

		if (ops[cpu][i] == 1) *npage_alloc = *npage_alloc + 1;
		else *npage_1024 = *npage_1024 + 1;
	}

	if (!nmaps) {
		nmaps = i;
	} else {
		assert(nmaps == i);
	}

	for (i = 0; i < nmaps; i++) {
		s = tsc_start();
		ret = call_cap(MMAN_RELEASE, cos_spd_id(), allmem[cpu][i], 0, 0);
		e = tsc_start();
#ifndef MEAS_MAP
		if (ops[cpu][i] == 1) {
			*tot = *tot + (e-s);
			if (cpu == 0) logs[nlog++] = (unsigned long)(e-s);
		} else {
			*tot_1024 = *tot_1024 + (e-s);
			if (cpu == 0) logs_large[nlog_large++] = (unsigned long)(e-s);
		}
#endif
		if (ret) {
			printc("cpu %d release error %d\n", cpu, ret);
			goto done;
		}
	}
done:
	return;
}

void cos_init(void)
{
	/* if (received[cos_cpuid()].snd_thd_created) { */
	/* 	rcv_thd(); */
	/* 	BUG(); */
	/* 	return; */
	/* } */
	/* received[cos_cpuid()].snd_thd_created = 1; */

	/* cap_switch_thd(RCV_THD_CAP_BASE + captbl_idsize(CAP_THD)*cos_cpuid()); */

	//init rcv thd first.

	/* if (cos_cpuid() == 0) { */
	/* 	pingpong(); */
	/* 	goto done; */
	/* } */

	arcv_ready[cos_cpuid()] = 1;
/* #if NUM_CPU <= 2 */
/* 	pingpong(); */
/* #else */
	int ret, cpu = cos_cpuid(); 
	unsigned long i;
	unsigned long long s,e, tot = 0, tot2 = 0;

	if (cos_cpuid() == 0) {
		printc(">>>>> calling mm init\n");
		call_cap(4, 0, 0, 0, 0);
		printc(">>>>> done mm init\n");
	}
	sync_all();

	if (mm_meas()) goto done;

	int repeat;
	unsigned long nops = 0, nops2 = 0;

	play_trace(&nops, &tot, &nops2, &tot2);
	/* workaround for the liveness id issue */
	tlb_quiescence_wait();
	sync_all();

	if (cos_cpuid()) goto done;

	if (cos_cpuid() == 0) nlog = 0;

	nops = tot = 0;
	nops2 = tot2 = 0;
	for (repeat = 0; repeat < 10; repeat++) {
		play_trace(&nops, &tot, &nops2, &tot2);
	}

	if (nops) {
		printc("nops %d cpu %d avg cost %llu (%lu ops), avg2 cost %llu (%lu ops)\n", 
		       NOPS, cpu, (tot)/(nops), nops, tot2/nops2, nops2);
	} else
		printc("cpu %d no ops\n", cpu);

	if (cos_cpuid() == 0) {
		unsigned long j, temp;
		for (i = 0; i < nlog; i++) {
			for (j = i + 1; j < nlog; j++) {
				if (logs[i] < logs[j]) {
					temp = logs[i];
					logs[i] = logs[j];
					logs[j] = temp;
				}
			}
		}

		for (i = 0; i < nlog_large; i++) {
			for (j = i + 1; j < nlog_large; j++) {
				if (logs_large[i] < logs_large[j]) {
					temp = logs_large[i];
					logs_large[i] = logs_large[j];
					logs_large[j] = temp;
				}
			}
		}

		printc("nlog %lu on core 0, SINGLE PAGE 99p (%lu) is %lu , 1024 page (%lu ops) 99p is %lu\n", 
		       nlog, nlog/100, logs[nlog/100], nlog_large, logs_large[nlog_large/100]);
#ifdef MEAS_MAP
		printc("Operation: mmap on %d cores\n", NUM_CPU_COS);
#else
		printc("Operation: munmap on %d cores\n", NUM_CPU_COS);
#endif
	}
/* #endif */
done:
	sync_all();
	cap_switch_thd(SCHED_CAPTBL_ALPHATHD_BASE + cos_cpuid()*captbl_idsize(CAP_THD));

	/* help linking. hack for now. */
	mman_get_page(0,0,0);
	mman_valloc(0,0,0);
	mman_revoke_page(0,0,0);
	mman_release_page(0,0,0);
	mman_alias_page(0,0,0,0,0);

	call();

	return;
}
