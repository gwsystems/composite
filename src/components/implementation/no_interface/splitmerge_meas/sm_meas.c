#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <evt.h>

volatile int evt, n_wait;

volatile u64_t c0_tsc = 0, c1_tsc = 0, c0_high = 0, c1_high = 0;

volatile int shared_mem[4], shared_ret;

#define HIGH_PRIO 20
#define LOW_PRIO (HIGH_PRIO+5)

void delay(int a) {
	/* printc("thd %d, core %d delay...\n", cos_get_thd_id(), cos_cpuid()); */

	volatile int b = 10000, c = 1;
	for ( ; a > 0; a--) {
		b = 10000;
		for( ; b > 0; b --) {
			c = 0;
		}
	}
	/* printc("thd %d, core %d delay done...\n", cos_get_thd_id(), cos_cpuid()); */
}

void create_thd(int core, int prio) {
	union sched_param sp, sp1;
	sp.c.type = SCHEDP_PRIO;
	sp.c.value = prio;

	sp1.c.type = SCHEDP_CORE_ID;
	sp1.c.value = core;
	if (sched_create_thd(cos_spd_id(), sp.v, sp1.v, 0) == 0) BUG();
}

#define ITER 1024
u32_t data[ITER];

void output() {
	u64_t start, end, avg, tot = 0, dev = 0;
	int i, j;

	for (i = 0 ; i < ITER ; i++) tot += data[i];
	avg = tot/ITER;
	printc("avg %lld\n", avg);
	for (tot = 0, i = 0, j = 0 ; i < ITER ; i++) {
		printc("i %u\n", data[i]);
		if (data[i] < avg*2) {
			tot += data[i];
			j++;
		}
	}
	printc("avg w/o %d outliers %lld\n", ITER-j, tot/j);

	for (i = 0 ; i < ITER ; i++) {
		u64_t diff = (data[i] > avg) ? 
			data[i] - avg : 
			avg - data[i];
		dev += (diff*diff);
	}
	dev /= ITER;
	printc("deviation^2 = %lld\n", dev);

}

void core1_high() {
	printc("core %ld high prio thd %d running.\n", cos_cpuid(), cos_get_thd_id());

	create_thd(0, HIGH_PRIO);

	create_thd(1, LOW_PRIO);

	/* Brand operations removed. Add acap creation here. */
	int received_ipi = 0;

	int param[4];
	u64_t s, e;
	int iter = 0;
	while (1) {
		int ret = 0;
		/* printc("core %ld going to wait, thd %d\n", cos_cpuid(), cos_get_thd_id()); */

		/* if (-1 == (ret = cos_areceive(...))) BUG(); */

		/* printc("core %ld, rec %d\n", cos_cpuid(), ++received_ipi); */
		param[0] = shared_mem[0];
		param[1] = shared_mem[1];
		param[2] = shared_mem[2];
		param[3] = shared_mem[3];
		assert(param[0] == 2);
		assert(param[1] == 4);
		assert(param[2] == 6);
		assert(param[3] == 8);

		/* rdtscll(e); */
		/* data[iter++] = e - c1_tsc; */

		int i;
		for (i = 0; i < n_wait; i++) {
			delay(20);
			/* printc("core %d triggering evt %d, i %d....\n", cos_cpuid(), evt, i); */
			shared_ret = 10;

			/* rdtscll(s); */

			evt_trigger(cos_spd_id(), evt);
 
			/* rdtscll(e); */
			/* data[iter++] = e - s; */

			/* printc("core %d triggerred evt %d, i %d....\n", cos_cpuid(), evt, i); */
		}
	}
}

void core0_high() {
	printc("core %ld high prio thd %d running.\n", cos_cpuid(), cos_get_thd_id());

	create_thd(0, LOW_PRIO); // creates low on core 0

	int i = 0;
	n_wait = 1;

	delay(20);

	evt = evt_create(cos_spd_id());
	printc("core %ld created evt %d....\n", cos_cpuid(), evt);
	
	int my_ret;
	u64_t s, e;
	while (i < ITER) {
		/* rdtscll(s); */

		shared_mem[0] = 2;
		shared_mem[1] = 4;
		shared_mem[2] = 6;
		shared_mem[3] = 8;
		/* add async inv here. */
		
		/* rdtscll(e); */
		/* data[i] = e - s; */

		/* printc("core %d ipi sent, going to evt_wait....\n", cos_cpuid()); */

		/* rdtscll(c0_high); */

		int ret = evt_wait_n(cos_spd_id(), evt, n_wait);
		my_ret = shared_ret;

		rdtscll(e);
		data[i] = e - c0_tsc;

		assert(my_ret == 10);
		/* printc("core %d up from evt_wait....\n", cos_cpuid()); */
		i++;
	}
	printc("core %ld test done, going to output...\n", cos_cpuid());
	output();
	printc("core %ld output done.\n", cos_cpuid());
}

/* evt_wait first half meas */

/* void core0_low() { */
/* 	printc("core %ld low prio thd %d running.\n", cos_cpuid(), cos_get_thd_id()); */
/* 	u64_t old = 0; */
/* 	int i = 0; */
/* 	while (1) { */
/* 		if (old != c0_high) { */
/* 			rdtscll(c0_tsc); */
/* 			//printc("old %llu, high %llu, now %llu\n", old, c0_high, c0_tsc); */
/* 			//assert(c0_tsc > c0_high); */
/* 			data[i++] = c0_tsc - c0_high; */
/* 			old = c0_high; */
/* 		} */
/* 	} */
/* } */

void core0_low() {
	while (1) {
		rdtscll(c0_tsc);
	}
}

void core1_low() {
	printc("core %ld low prio thd %d running.\n", cos_cpuid(), cos_get_thd_id());
	while (1) {
		rdtscll(c1_tsc);
	}
}

void cos_init(void) {
	static int first = 1;

	printc("thd %d, core %ld in pong\n", cos_get_thd_id(), cos_cpuid());

	if (first) {
		first = 0;
		create_thd(1, HIGH_PRIO);
		printc("thd %d, core %ld done init in pong\n", cos_get_thd_id(), cos_cpuid());

		return;
	}

//core 1
	static int first_core1 = 1;
	if (cos_cpuid() == 1) {
		if (first_core1 == 1) {
			first_core1 = 0;
			core1_high();
		} else {
			core1_low();
		}
		return;
	}
	
//core 0
	static int first_core0 = 1;
	if (cos_cpuid() == 0) {
		if (first_core0 == 1) {
			first_core0 = 0;
			core0_high();
		} else {
			core0_low();
		}
		return;
	}
	
	return;
}
