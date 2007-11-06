#include <cos_component.h>

const char comp1str[] = "Hello World from component 1!\n";

extern int spd2_fn(void);

extern int print(void);
extern int print_vals(int a, int b, int c);
extern void yield(void);

static inline int find_size(const char *m)
{
	int sz;

	for (sz = 0 ; m[sz] != '\0' ; sz++) ;

	return sz;
}

int call_print(const char *m)
{
	int sz;
	int *sz_ptr = COS_FIRST_ARG;
	char *m_ptr = (char *)(sz_ptr + 1); // plus 4 bytes 

	sz = find_size(comp1str);
	*sz_ptr = sz;
	cos_memcpy(m_ptr, comp1str, sz);

	return print();
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

void cos_upcall_fn(vaddr_t data_region, int thd_id, 
		   void *arg1, void *arg2, void *arg3)
{
	curr = thd_id;

	while (1) {
		yield();
		//	c1_yield();
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
	print_vals(0,0,0);
}
