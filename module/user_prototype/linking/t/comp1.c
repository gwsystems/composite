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
	unsigned long long now, prev;
	int i;

	if (first) {
		first = 0;
		//print_vals(1,2,3);
	}
	spd2_fn();

	rdtscll(now);
	for (i = 0 ; i < 2000000000 ; i++) {
		if (turn == 0) {
			prev = now;
			rdtscll(now);
			print_vals(i, turn, (unsigned int)(now-prev));
			turn = 1;
		}
		//nothing();
		//yield();
	}
	
	return blah;
}

void cos_upcall_fn(vaddr_t data_region, int thd_id, 
		   void *arg1, void *arg2, void *arg3)
{
	//print_vals(3, 2, 1);
	int cnt = 0, i;
	unsigned long long now, prev;

	if (thd_id != 1) {
		print_vals(9, 9, 9);
	}

	rdtscll(now);
	while (1) {
		//nothing();
		
		if (turn == 1) {
			prev = now;
			rdtscll(now);
 			print_vals(cnt, turn, (unsigned int)(now-prev));
			turn = 0;
		}
		cnt++;

		//if (cnt % 10000000 == 0) print_vals(cnt, turn, 2);
		//yield();
	}

	return;
}

void symb_dump(void)
{
	/* crap symbols issue, remove */
	yield();
	nothing();
}
