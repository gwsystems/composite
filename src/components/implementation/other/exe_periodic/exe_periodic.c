/**
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
#include <sched_conf.h>
#include <exe_synth_hier.h>

int period = 100, num_invs = 1;

#define US_PER_TICK 10000

int exe_t = 80;  /* in us,less than 2^32/(2.33*10^9/1000) which is 1843 ms on 2.33GHz machine */

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
     case 'p':		/* spin */
	  period = atoi(++d);
	  break;
     case 'n':		/* num of invocations */
	  num_invs = atoi(++d);
	  break;
     case 'e':		/* execution time */
	  exe_t = atoi(++d);
	  break;
     }

     return s;
}

int parse_initstr(void)
{
     struct cos_array *data;
     char *c;

     data = cos_argreg_alloc(sizeof(struct cos_array) + 52);
     assert(data);
     data->sz = 52;
	
     if (sched_comp_config_initstr(cos_spd_id(), data)) {
	  printc("No initstr found.\n");
	  return -1;
     }

     c = data->mem;
     while ('\0' != *c) c = parse_step(c);
	
     cos_argreg_free(data);

     return 0;
}

volatile u64_t touch;
volatile int k;
void cos_init(void *arg)
{

     parse_initstr();

     if (period < 1 || (exe_t > period*US_PER_TICK)) BUG();

     periodic_wake_create(cos_spd_id(), period);

     unsigned long cyc_per_tick;
     cyc_per_tick = sched_cyc_per_tick();

     unsigned long exe_cycle;
     exe_cycle = cyc_per_tick/US_PER_TICK;
     exe_cycle = exe_cycle*exe_t;

     printc("Thd %d, period %d ticks, execution time %d us in %lu cycles\n", cos_get_thd_id(), period, exe_t, exe_cycle);

     /* Allow all periodic tasks to begin */
     int i = 0, waiting = 50 / period;
     do {
	     periodic_wake_wait(cos_spd_id());
     } while (i++ < waiting); /* wait 50 ticks */

     unsigned long exe_cyc_remained = 0;

//		     printc("%d start!!\n",cos_get_thd_id(),exe_cyc_remained);
     while (1) {
	     
	     //rdtscll(start);
	     exe_cyc_remained = exe_cycle;  /* refill */
	     while(exe_cyc_remained) {
		     exe_cyc_remained = left(exe_cyc_remained,exe_cycle);	  
//		     printc("%d,i'm back!!%lu\n",cos_get_thd_id(),exe_cyc_remained);
	     }
	     //rdtscll(end);
	     //printc("thd: %d  time: %llu\n",cos_get_thd_id(),end-start);
//	     printc("%d, times : %d\n", cos_get_thd_id(), times);
	     periodic_wake_wait(cos_spd_id());
	  
     }
     return;
}

void bin (void)
{
     sched_block(cos_spd_id(), 0);
}
