#include <cos_component.h>
#include <synth_hier.h>

#include <sched.h>
#include <print.h>

#include <stdlib.h>

#define TOTAL_AMNT 128		/* power of 2 */

int parsed = 0;
unsigned int spin = 0, l_to_r = 64, num_invs = 1;

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

void parse_initstr(void)
{
//	struct cos_array *data;
	char *c;
	
	if (parsed) return;
	parsed = 1;

	c = cos_init_args();
	while ('\0' != *c) c = parse_step(c);

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
}

volatile int v = 0;

extern void calll_left(void);
extern void callr_right(void);

void do_action(void)
{
	unsigned int i, j, val;
	u64_t t;

	parse_initstr();	

	for (j = 0 ; j < num_invs ; j++) {
		for (i = 0 ; i < spin ; i++, v++) ;

		rdtscll(t);
		val = (int)(t & (TOTAL_AMNT-1));
		if (val <= l_to_r) {
			calll_left();
		} else {
			callr_right();
		}
	}
}

void left(void)
{
	do_action();
}

void right(void)
{
	do_action();
}

void cos_init(void)
{
	while (1) do_action();
}

void bin(void)
{
	sched_block(cos_spd_id(), 0);
}
