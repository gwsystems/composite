/*
 * Copyright 2009 by Gabriel Parmer, gparmer@gwu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <stdlib.h>

#include <cos_component.h>
#include <print.h>

#include <periodic_wake.h>
#include <sched.h>
#include <exe_cbuf_synth_hier.h>
#include <timed_blk.h>

int period = 100;
int start_time = 0, duration_time = 120;
int priority = 0;
int thd_num = 0;

#define US_PER_TICK 10000

int exe_t = 80;  /* in us,less than 2^32/(2.33*10^9/1000) which is 1843 ms on 2.33GHz machine */

#define TOTAL_AMNT 128		/* power of 2 */

#define NUM_LOOPS 1000

volatile unsigned long kkk = 0;
unsigned long loop_cost = 0;
unsigned long get_loop_cost(unsigned long loop_num)
{
	u64_t start,end;
	unsigned long int i;

	kkk = 0;
	rdtscll(start);
	for (i=0;i<loop_num;i++) kkk++;   /* Make sure that -O3 is on to get the best result */
	rdtscll(end);

	return (end-start)/loop_num;  /* avg is 6 per loop */
}

char *parse_step(char *d)
{
	char *s = strchr(d, ' ');
	if (!s) {
		if ('\0' == d) return d;
		s = d + strlen(d);
	} else {
		*s = '\0';
		s++;
	}

	switch(*d) {
	case 'a':
		priority = atoi(++d);
		break;
	case 'p':		/* spin */
		period = atoi(++d);
		break;
	case 'e':		/* execution time in us */
		exe_t = atoi(++d);
		break;
	case 's':		/* start running time in sec */
		start_time = atoi(++d);
		break;
	case 'd':		/* duration time in sec */
		duration_time = atoi(++d);
	case 'n':
		thd_num = atoi(++d);
		break;
	}

	return s;
}

int parse_initstr(void)
{
//	struct cos_array *data;
	char *c;

	c = cos_init_args();
	while ('\0' != *c) c = parse_step(c);

	/* data = cos_argreg_alloc(sizeof(struct cos_array) + 52); */
	/* assert(data); */
	/* data->sz = 52; */
	
	/* if (sched_comp_config_initstr(cos_spd_id(), data)) { */
	/* 	printc("No initstr found.\n"); */
	/* 	return -1; */
	/* } */

	/* c = data->mem; */
	/* while ('\0' != *c) c = parse_step(c); */
	
	/* cos_argreg_free(data); */

	return 0;
}


static int create_thd(const char *pri)
{
	struct cos_array *data;
	int event_thd = 0;
	int sz = strlen(pri) + 1;
    
	data = cos_argreg_alloc(sizeof(struct cos_array) + sz);
	assert(data);
	strcpy(&data->mem[0], pri);
	//data->sz = 4;
	data->sz = sz;

	assert(0);// TODO: use create_thd
//	if (0 > (event_thd = sched_create_thread(cos_spd_id(), data))) assert(0);
	cos_argreg_free(data);
    
	return event_thd;
}

/* #define BEST_EFF */

volatile u64_t touch;
volatile int k;

void cos_init(void *arg)
{

	int start_time_in_ticks = 0;
	int duration_time_in_ticks = 0;

	int local_period = 0;

	static int first = 1;

//	static int pre_run = 0;

	if (first) {
		union sched_param sp;
		int i;
		first = 0;
		parse_initstr();
		assert(priority);
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = priority;

		if (sched_create_thd(cos_spd_id(), sp.v, 0, 0) == 0) BUG();
		if (priority == 30) { //best effort thds
			printc("thd num %d\n",thd_num);
				for (i=0; i<(thd_num-1); i++)
				sched_create_thd(cos_spd_id(), sp.v, 0, 0);
		}
		return;
	}

	local_period = period;

	unsigned long cyc_per_tick;
	cyc_per_tick = sched_cyc_per_tick();

	unsigned long exe_cycle;
	exe_cycle = cyc_per_tick/US_PER_TICK;
	exe_cycle = exe_cycle*exe_t;

	start_time_in_ticks = start_time*100;
	duration_time_in_ticks = duration_time*100;

	printc("In spd %ld Thd %d, period %d ticks, execution time %d us in %lu cycles\n", cos_spd_id(),cos_get_thd_id(), local_period, exe_t, exe_cycle);

	/* int event_thd = 0; */
	/* unsigned long pre_t_0 = 0; */
	if (local_period <= 0){/* Create all non-periodic tasks */

#ifdef BEST_EFF   // for best effort sys now
		int i;
		if (first == 0){
//			for (i= 0;i<5;i++) create_thd("r0");
			pre_t_0 = sched_timestamp();
			first = 1;
		}

		printc("<<<<1 thd %d in spd %ld\n",cos_get_thd_id(), cos_spd_id());
		event_thd = cos_get_thd_id();

		for (i = 0 ; i < 100; i++){
			left(200000,200000,0,0);
		}

		unsigned long pre_run_remained = 0;
		unsigned long numm = 10000*cyc_per_tick/US_PER_TICK;;
		while(1) {
			for (i = 0 ; i < 10; i++){
				pre_run_remained = numm;  /* refill */
				pre_run_remained = left(20000,20000,0,0);
				/* printc(" thd %d pre_t_0 %u pre_t %lu\n", cos_get_thd_id(), pre_t_0, pre_t); */
			}
			unsigned long pre_t = sched_timestamp();
			if ( pre_t > pre_t_0 + 1*100) break;
		}
		printc("BF thd %d finish pre_run\n", cos_get_thd_id());

#endif
		/* pub_duration_time_in_ticks = duration_time_in_ticks; */
		timed_event_block(cos_spd_id(), start_time_in_ticks);
		printc("<<<<2 thd %d in spd %ld\n",cos_get_thd_id(), cos_spd_id());
	}
	else {/* Create all periodic tasks */
		if (local_period == 0 || (exe_t > local_period*US_PER_TICK)) BUG();
		/* if (cos_get_thd_id() == 20) { */
		/* 	printc("pre allocating ...\n"); */
		/* 	int mm; */
		/* 	for (mm = 0 ; mm < 3000; mm++) */
		/* 		left(70000, 70000, 0, 0); */
		/* 	printc("done.\n"); */
		/* } */
		periodic_wake_create(cos_spd_id(), local_period);

		int i = 0;
		int waiting = 0;

		if(start_time_in_ticks <= 0)
			/* waiting = (50+100*10) / local_period;   /\* use 50 before. Now change to let BF threads run first 10 seconds before 0 second *\/ */
			waiting = (50) / local_period;   /* use 50 before. Now change to let BF threads run first 10 seconds before 0 second */
		else
			waiting = start_time_in_ticks / local_period;
		do {
			periodic_wake_wait(cos_spd_id());
		} while (i++ < waiting); /* wait 50 ticks */
	}

/* Let all tasks run */
	unsigned long exe_cyc_remained = 0;
//	unsigned long long t;
//	unsigned long val;
	int refill_number = 0;

	unsigned long exe_cyc_event_remained = 0;
//	printc("start...!!\n");
	while (1) {
		if(local_period <= 0){			/* used for transient non-periodic tasks only */
			exe_cyc_event_remained = exe_cycle;  /* refill */
			while(1) {
				exe_cyc_event_remained = exe_cycle;  /* refill */
				/* rdtscll(t); */
				/* val = (int)(t & (TOTAL_AMNT-1)); */
				/* if (val >= 64){ */
					exe_cyc_event_remained = left(exe_cyc_event_remained,exe_cycle,0,0);
				/* } */
				/* else{ */
				/* 	exe_cyc_event_remained = right(exe_cyc_event_remained,exe_cycle,0,0); */
				/* } */
				unsigned long t = sched_timestamp();
				/* if ( t > (unsigned long)(7*100 + start_time_in_ticks + duration_time_in_ticks)) { */
				/* 	printc("thd %d left!!!\n",cos_get_thd_id()); */
				if ( t > (unsigned long)( start_time_in_ticks + duration_time_in_ticks)) {
					/* printc("thd %d left>>>\n",cos_get_thd_id()); */

					timed_event_block(cos_spd_id(), 10000);
				}
			}
		}
		else{
			/* rdtscll(start); */
			/* used for transient periodic tasks only */
			if (start_time_in_ticks > 0 && (local_period*refill_number > duration_time_in_ticks)){
				for(;;) periodic_wake_wait(cos_spd_id());
			}
			exe_cyc_remained = exe_cycle;  /* refill */
			/* printc("thd %d in home comp, going to call\n",cos_get_thd_id()); */
			while(exe_cyc_remained) {
				exe_cyc_remained = left(exe_cyc_remained,exe_cycle,0,0);	  
			}
			
			/* rdtscll(end); */
			/* printc("%d, times : %d\n", cos_get_thd_id(), times); */
			/* printc("\n @@thd %ld is sleeping in spd %d\n", cos_get_thd_id(), cos_spd_id()); */
			/* printc("thd %d back in home comp, going to block\n",cos_get_thd_id()); */
			periodic_wake_wait(cos_spd_id());
			/* printc("thd %d woke up.\n",cos_get_thd_id()); */
			refill_number++;	  
			/* printc("\n @@thd %ld is waking in spd %d\n", cos_get_thd_id(), cos_spd_id()); */
			/* printc("\n thd %d refilled...%d\n", cos_get_thd_id(), refill_number); */
		}
	}
	return;
}

