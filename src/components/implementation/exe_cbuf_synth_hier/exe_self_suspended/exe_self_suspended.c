#include <cos_component.h>
#include <sched.h>
#include <print.h>
#include <cbuf.h>
#include <exe_self_suspension.h>
#include <timed_blk.h>

#define MAXULONG (~0)

#include <stdlib.h>

#define TOTAL_AMNT 128		/* power of 2 */

unsigned int ss_time = 1000;

#define AVG_INVC_CYCS 1000   /* From the measurement */

#define NUM_LOOPS 10000

#define PERCENT_EXE 1

volatile unsigned long kkk = 0;
unsigned long loop_cost = 0;


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
	case 't':		/* suspension time */
		ss_time = atoi(++d);
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
	c = cos_init_args();
	while ('\0' != *c) c = parse_step(c);
	
//	cos_argreg_free(data);
}


/* only consume 1% computation unless exe_time_left is 0 */
unsigned long do_something (unsigned long exe_time_left, unsigned long const initial_exe_t)
{
	unsigned long has_run, i;
	unsigned long ss = initial_exe_t / (100 / PERCENT_EXE) / 6;
	kkk = 0;

	for (i=0; i<ss; i++) kkk++;

	has_run = ss * 6;

	if (has_run >= exe_time_left) {
		return 0;
	}
	exe_time_left -= has_run;

	return exe_time_left;
}


unsigned long ss_action(unsigned long exe_time_left, unsigned long const initial_exe_t)
{

	parse_initstr();

	/* printc(">> Now I thd %d am in ss spd %d\n",cos_get_thd_id(),cos_spd_id()); */

	if (AVG_INVC_CYCS > exe_time_left) return 0;
	exe_time_left -= AVG_INVC_CYCS;
	if (exe_time_left == 0) return 0;

	exe_time_left = do_something(exe_time_left, initial_exe_t);
	if (exe_time_left == 0) return 0;
		
	timed_event_block(cos_spd_id(), ss_time);  /* blocked for some ticks  */

	exe_time_left -= AVG_INVC_CYCS;
	if (exe_time_left == 0) return 0;

	exe_time_left = do_something(exe_time_left, initial_exe_t);

	return exe_time_left;
}


unsigned long left(unsigned long exe_t, unsigned long const  initial_exe_t, cbuf_t cbt, int len)
{
	return -1;
}

unsigned long right(unsigned long exe_t,  unsigned long const initial_exe_t, cbuf_t cbt, int len)
{
	return -1;
}


