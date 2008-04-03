#include <cos_component.h>
#include <cos_alloc.h>
#include <cos_scheduler.h>
#include <print.h>

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
volatile static int nothing_var = 0;

void yield(void)
{
	unsigned short int prev_thd; 

 	//prev_thd = cos_get_thd_id();
	prev_thd = cos_get_thd_id();

	if (prev_thd == thd_id) {
		curr_thd = other_thd;
	} else {
		other_thd = prev_thd;
		curr_thd = thd_id;
	}

	cos_switch_thread(curr_thd, 0/*COS_THD_SCHED_RETURN*/, 0);

	return;
}

int test_locks_fn(void);
__attribute__((regparm(1)))
void thread_tramp(void *data)
{
	int c_id = (int)data, test_id;

	test_id = cos_get_thd_id();

	if (test_id != c_id) {
		print("error in thd tramp: %d %d %d", test_id, c_id, 0);
	}

	test_locks_fn(); 

	while (1) cos_switch_thread(1, 0, 0);

	cos_upcall(1);

	/* never get here */
	while (1) {
		yield();
	}
}

#define LOWER 2
#define UPPER 3

void run_thds(void)
{
	cos_switch_thread(new_thd1, 0, 0);
	//print_vals(6,6,6,6);
	cos_switch_thread(new_thd2, 0, 0);
}

int spd2_fn(void)
{
	unsigned int i;

	//print_vals(1, 2, 3, 4);
	//print_vals(cos_get_thd_id(), (int)cos_get_arg_region(), sizeof(f), 4321);
	//print_vals((int)&i,6,6);
	thd_id = cos_get_thd_id();
	curr_thd = thd_id;

	new_thd1 = cos_create_thread(thread_tramp, get_stack_addr(LOWER), (void*)LOWER);
	//print_vals(new_thd1, new_thd1, new_thd1, new_thd1);
	if (new_thd1 != LOWER) {
		print("error: spd2_fn w/ %d %d %d", new_thd1, 1, 1);
	}

	new_thd2 = cos_create_thread(thread_tramp, get_stack_addr(UPPER), (void*)UPPER);
	//print_vals(new_thd2, new_thd2, new_thd2, new_thd2);

	if (new_thd2 != UPPER) {
		print("error: spd2_fn w/ %d %d %d", new_thd2, 1, 1);
	}

	other_thd = new_thd1;
	run_thds();
	//bthd = cos_brand(0, COS_BRAND_CREATE);
	//cos_brand(bthd, COS_BRAND_ADD_THD);

 	for (i = 0 ; i < 1000000 ; i++) {
		//print_vals(5,6,7,8);
		yield();
	}

	return 1234;
}

#define ITER 1000000

int run_demo(void)
{
	int i;

	for (i = 0 ; i < ITER ; i++) {
		//print_vals(7337, 0, 2);
		cos_mpd_cntl(COS_MPD_DEMO);
		//print_vals(7337, 1, 2);
	}
	return i;
}

#define OTHER_THD(x) (((x) == 1)? 2: 1)
volatile static int race_val;
#define SPIN 10

/* use stack space and registers */
int fact(int x) {
	if (x == 1 || x == 2) return 1;
	return fact(x-1) + fact(x-2);
}

int test_locks_fn(void)
{
	unsigned long long cnt = 0;
	while (1) {
		int tmp, i = 0;
		if (cos_sched_lock_take() == -1) {
//			print_vals(9, 7, cos_sched_notifications.locks.owner_thd, cos_sched_notifications.locks.queued_thd);
			print("error: could not take lock %d%d%d",0,0,8);
			return -1;
		}
		tmp = race_val;

//		print_vals(3,2,1,3); 
		fact(40);
		while (i++ < SPIN);

		if ((cnt & ((2<<12)-1)) == 0) cos_switch_thread(OTHER_THD(cos_get_thd_id()), 0, 0);

		if (race_val != tmp) print("error: race condition %d%d%d",0,0,8);
		if (-1 == cos_sched_lock_release()) {
			print("error: could not release lock %d%d%d",0,0,8);
			return -1;
		}
		//cos_switch_thread(OTHER_THD(cos_get_thd_id()), 0, 0);
		cnt++;
	}
}

int test_locks(void)
{
	unsigned int new_thd1;//, new_thd2;

	new_thd1 = cos_create_thread(thread_tramp, get_stack_addr(LOWER), (void*)LOWER);
//	new_thd2 = cos_brand_cntl(0, COS_BRAND_CREATE);
//	cos_brand_cntl(new_thd2, COS_BRAND_ADD_THD);

	//new_thd2 = cos_create_thread(thread_tramp, get_stack_addr(UPPER), (void*)UPPER);
	return test_locks_fn();
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	if (t == COS_UPCALL_BRAND_COMPLETE) {
		cos_switch_thread(1, COS_SCHED_TAILCALL, 0);
	}
	//print_vals(1,4,7,5);
	if (-1 == cos_sched_lock_take()) {
		print("error: in interrupt cannot take lock %d%d%d", 5, 5, 5);
		return;
	}
	race_val++;
	if (-1 == cos_sched_lock_release()) {
		print("error: in interrupt cannot release lock %d%d%d", 5, 5, 5);
		return;
	}

	nothing_var++;
	return;
}

#define SZ 128
#define INC (PAGE_SIZE>>8)
#define NP 4

//extern void *mman_get_page(spdid_t spd, void *addr, int flags);
//extern void mman_release_page(spdid_t spd, void *addr, int flags);
int test_mmapping(void)
{
	unsigned long *ptrs[SZ];
	unsigned long *pages[NP];
	int i, j;

	for (j = 0 ; j < 2 ; j++) {
		for (i = 0 ; i < SZ/4 ; i++) {
			int idx = i*4;
			ptrs[idx]   = malloc(4+(INC*i)); *ptrs[idx]   = 0xdeadbeef;
			ptrs[idx+1] = malloc(4+(INC*i)); *ptrs[idx+1] = 0xdeadbeef;
			ptrs[idx+2] = malloc(4+(INC*i)); *ptrs[idx+2] = 0xdeadbeef;
			ptrs[idx+3] = malloc(4+(INC*i)); *ptrs[idx+3] = 0xdeadbeef;
		}
		
		for (i = 0 ; i < SZ/4 ; i++) {
			int idx = i*4;
			if (*ptrs[idx] != 0xdeadbeef ||
			    *ptrs[idx+1] != 0xdeadbeef ||
			    *ptrs[idx+2] != 0xdeadbeef ||
			    *ptrs[idx+3] != 0xdeadbeef) {
				print("error mapping %d%d%d", 505, 505, 505);
			}
			    
			free(ptrs[idx]);
			free(ptrs[idx+1]);
			free(ptrs[idx+2]);
			free(ptrs[idx+3]);
		}
	}

	for (j = 0 ; j < 2 ; j++) {
		for (i = 0 ; i < NP ; i++) {
			pages[i] = alloc_page();
		}
		for (i = 0 ; i < NP ; i++) {
			 free_page(pages[i]);
		}
	}
	
/*	print_vals(cos_get_heap_ptr(),1,1,1);
	a = mman_get_page(cos_spd_id(), cos_get_heap_ptr(), 0);
	*(long*)a = 56789;
	print_vals(a,*(long*)a,2,2);
	mman_release_page(cos_spd_id(), cos_get_heap_ptr(), 0);
	print_vals(3,3,3,3);
*/
		//cos_set_heap_ptr(cos_get_heap_ptr() + PAGE_SIZE)
	return 0;
}

int test_print(void)
{
	print("foo %x %x %x", 1, 2, 3);	
	print("foo %x %x %x", 4, 5, 6);
	
	return 0;
}

int sched_init(void)
{
	int ret;

	//print_vals((int)&ret,6,6);
//	ret = run_demo();
//	ret = spd2_fn();
//	ret = test_mmapping();
//	ret = test_locks();
	ret = test_print();
	//print_vals(6,6,6);
	nothing_var++;

	return ret;
}

void symb_bag(void)
{
	print("%d%d%d", 0, 0, 0);
}
