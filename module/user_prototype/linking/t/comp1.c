#include <cos_component.h>
#include <print.h>

const char comp1str[] = "Hello World from component 1!\n";

extern int spd2_fn(void);

extern void yield(void);

static inline int find_size(const char *m)
{
	int sz;

	for (sz = 0 ; m[sz] != '\0' ; sz++) ;

	return sz;
}

extern void nothing(void);

int blah = 0;
volatile int turn = 0;
int spd1_fn(void *data, int thd_id)
{
	static int first = 1;
	unsigned long long now;
	int i;

	if (first) {
		first = 0;
		//print_vals(1,2,3);
	}
	spd2_fn();

	rdtscll(now);
	for (i = 0 ; i < 2 ; i++) {
/*		if (turn == 0) {
			prev = now;
			rdtscll(now);
			print_vals(i, turn, (unsigned int)(now-prev));
			turn = 1;
		}
*/
		//nothing();
		yield();
	}
	
	return blah;
}

#define LOWER 2
#define UPPER 3

volatile unsigned short int curr = 0;

void c1_yield()
{
	curr = (curr == LOWER)? UPPER : LOWER;
	cos_switch_thread(curr, 0, 0);	
	//print_vals(cos_get_thd_id(), curr, 1);
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	curr = cos_get_thd_id();

	//print_vals(cos_get_thd_id(), cos_spd_id(), 0, 0);
	while (1) {
		yield();
		c1_yield();
/*		c1_yield();
		c1_yield();
		c1_yield();*/
	}

	return;
}

void symb_dump(void)
{
	/* crap symbols issue, remove */
	yield();
	nothing();
	print("%d%d%d",0,0,0);
}
