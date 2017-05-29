#include <cos_component.h>
#include <exe_synth_hier.h>

#include <sched.h>
#include <print.h>

#define MAXULONG (~0)

#include <stdlib.h>

#define TOTAL_AMNT 128		/* power of 2 */

unsigned int spin = 1000, l_to_r = 64, num_invs = 1;

#define AVG_INVC_CYCS 1000   /* From the measurement */

#define NUM_LOOPS 10000

#define PERCENT_EXE 10

volatile unsigned long kkk = 0;
unsigned long loop_cost = 0;

static inline void do_loop(unsigned long iter)
{
	unsigned long i;

 	for (i=0;i<iter;i++) kkk++;
}

static unsigned long get_loop_cost(unsigned long loop_num)
{
	u64_t start,end;

	rdtscll(start);
	do_loop(loop_num);
	rdtscll(end);

	return end-start; 
}

static char *parse_step(char *d)
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
	case 's':		/* spin */
		spin = atoi(++d);
		break;
	case 'r':		/* ratio */
		l_to_r = atoi(++d);
		if (l_to_r > TOTAL_AMNT) l_to_r = TOTAL_AMNT;
		break;
	case 'n':		/* num of invocations */
		num_invs = atoi(++d);
		break;
	}

	return s;
}

static void parse_initstr(void)
{
//	struct cos_array *data;
	char *c;
	static int parsed = 0;

	if (parsed) return;
	parsed = 1;

	/* data = cos_argreg_alloc(sizeof(struct cos_array) + 52); */
	/* assert(data); */
	/* data->sz = 52; */
	
	/* if (sched_comp_config_initstr(cos_spd_id(), data)) { */
	/* 	printc("No initstr found.\n"); */
	/* 	return; */
	/* } */
	/* //printc("%s\n", data->mem); */

	/* c = data->mem; */
	/* while ('\0' != *c) c = parse_step(c); */
	
	/* cos_argreg_free(data); */
	c = cos_init_args();
	while ('\0' != *c) c = parse_step(c);
}

extern unsigned long calll_left(unsigned long exe_t, unsigned long const initial_exe_t);
extern unsigned long callr_right(unsigned long exe_t, unsigned long  const initial_exe_t);

static unsigned long measure_loop_costs(unsigned long spin) 
{
	
	unsigned long temp = 0;
	temp = 6 * spin;
	loop_cost = temp;
	return temp;
	/*rdtscll(start);
	do {
		int i;
		min = MAXULONG;
		max= 0;
		for (i = 0 ; i < 10 ; i++) {
			temp = get_loop_cost(spin);
			if (temp < min) min = temp;
			if (temp > max) max = temp;
		}
	} while ((max-min) > (min/128));

	loop_cost = temp;
	rdtscll(end);
	assert(end>start);
	printc("spin:%lu, loopcost measurement :%lu\n",spin, temp );
	return temp;*/
}

static unsigned long do_action(unsigned long exe_time_left, const unsigned long initial_exe_t)
{

	unsigned long i, j, val;
	unsigned long long t;
	static int first = 1;

	unsigned long has_run;   /* thread has run cycles in this inv */

	parse_initstr();
//	printc("thd %d enter comp %d!\n", cos_get_thd_id(), cos_spd_id());
	if (first) {
		unsigned long temp = 0;
		temp = measure_loop_costs(spin);
		first = 0;
		/*if (exe_time_left < temp) return 0;
		  exe_time_left -= temp;*/
	}
	//printc("thd %d here1!\n", cos_get_thd_id());
	if (AVG_INVC_CYCS > exe_time_left) return 0;
	exe_time_left -= AVG_INVC_CYCS;

	for (j = 0 ; j < num_invs ; j++) {
		if (exe_time_left == 0) return 0;
		kkk = 0;

		/* for (i=0;i<spin;i++) kkk++;   */
		
		unsigned long ss = initial_exe_t / (100 / PERCENT_EXE) / 6;
		for (i=0; i<ss; i++) kkk++;
		has_run = ss * 6;//loop_cost;//

		if (has_run > exe_time_left) {
			return 0;
		}
		exe_time_left -= has_run;
                //printc("thd %d, has_run %lu, left: %lu!\n", cos_get_thd_id(),loop_cost, exe_time_left);
		rdtscll(t);

		val = (int)(t & (TOTAL_AMNT-1));
		if (val >= l_to_r) {
			exe_time_left = calll_left(exe_time_left, initial_exe_t );
		} else {
			exe_time_left = callr_right(exe_time_left, initial_exe_t );
		}
	}
//	printc("thd %d left comp %d!\n", cos_get_thd_id(), cos_spd_id());
	return exe_time_left;
}

unsigned long left(unsigned long exe_t, unsigned long const  initial_exe_t)
{
	return do_action(exe_t,initial_exe_t);
}

unsigned long right(unsigned long exe_t,  unsigned long const initial_exe_t)
{
	return do_action(exe_t,initial_exe_t);
}


void cos_init(void)
{

	while (1) do_action(10000,10000);
}

void bin(void)
{
	sched_block(cos_spd_id(), 0);
}
