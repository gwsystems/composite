#include <cos_component.h>

extern int print_vals(int a, int b, int c);

void nothing(void)
{
	return;
}

extern char cos_static_stack;
/* mirrored from cos_asm_upcall.S */
static inline vaddr_t get_stack_addr(int thd_id)
{
	/* -4 for the pop that will happen on syscall return */
	return (int)&cos_static_stack+(thd_id<<12)-4; 
}

static unsigned short int thd_id = 0, new_thd1 = 0, new_thd2 = 0;
volatile static unsigned short int curr_thd, other_thd;
static int nothing_var;

__attribute__((regparm(1)))
void yield(unsigned int thd)
{
	unsigned short int prev_thd; 

 	//prev_thd = cos_get_thd_id();
	//print_vals(thd, prev_thd, 0);
	prev_thd = thd;
	if (prev_thd == thd_id) {
		curr_thd = other_thd;
	} else {
		other_thd = prev_thd;
		curr_thd = thd_id;
	}

	cos_switch_thread(curr_thd, 0/*COS_THD_SCHED_RETURN*/, 0);
	//print_vals(cos_get_thd_id(), curr_thd, 2);
	//print_vals(curr_thd, 11, 11);
	return;
}

__attribute__((regparm(1)))
void thread_tramp(void *data)
{
	int c_id = (int)data, test_id;

	test_id = cos_get_thd_id();

	if (test_id != c_id) {
		print_vals(6, test_id, c_id);
	}

	cos_upcall(1);

	/* never get here */
	while (1) {
		yield(test_id);
	}
}

#define LOWER 2
#define UPPER 3

int spd2_fn(void)
{
	unsigned int i;

	//print_vals((int)&i,6,6);
	thd_id = cos_get_thd_id();
	curr_thd = thd_id;

	new_thd1 = cos_create_thread(thread_tramp, get_stack_addr(LOWER), (void*)LOWER);

	if (new_thd1 != LOWER) {
		print_vals(7, new_thd1, 1);
	}

	new_thd2 = cos_create_thread(thread_tramp, get_stack_addr(UPPER), (void*)UPPER);

	if (new_thd2 != UPPER) {
		print_vals(7, new_thd2, 1);
	}

	other_thd = new_thd1;
	cos_switch_thread(new_thd1, 0, 0);

	cos_switch_thread(new_thd2, 0, 0);

//	bthd = cos_brand(0, COS_BRAND_CREATE);
//	cos_brand(bthd, COS_BRAND_ADD_THD);

 	for (i = 0 ; i < 10000 ; i++) {
		yield(thd_id);
	}

	return 1234;
}

extern void bar(unsigned int val, unsigned int val2);

int run_demo(void)
{
	int i, j;

	for (i = 0 ; i < 6 ; i++) {
		for (j = 0 ; j < 5 ; j++) {
			bar(i, j);
		}
		//print_vals(7337, 0, 2);
		cos_mpd_cntl(COS_MPD_DEMO);
		//print_vals(7337, 1, 2);
	}
	
	return i;
}

void cos_upcall_fn(vaddr_t data_region, int id, 
		   void *arg1, void *arg2, void *arg3)
{
	yield(id);

	nothing_var += 1;
	return;
}

int sched_init(void)
{
	int ret;

	//print_vals((int)&ret,6,6);
	//ret = spd2_fn();
	ret = run_demo();
	
	//print_vals(6,6,6);
	nothing_var++;

	return ret;
}

void symb_bag(void)
{
	bar(0, 1);
}
