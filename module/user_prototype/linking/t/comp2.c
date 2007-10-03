#include <cos_component.h>

//#define PREEMPT
#define COOPERATIVE

extern int print_vals(int a, int b, int c);

void nothing(void)
{
	//print_vals(1, 1, 1);
	return;
}

extern char cos_static_stack;
/* mirrored from cos_asm_upcall.S */
static inline vaddr_t get_stack_addr(int thd_id)
{
	/* -4 for the pop that will happen on syscall return */
	return (int)&cos_static_stack+(thd_id<<12)-4; 
}

int thd_id = 0, new_thd = 0, curr_thd;

__attribute__((regparm(1)))
void thread_tramp(void *data)
{
	int c_id = (int)data, test_id;

//	print_vals(3, (int)data, 0);

	test_id = cos_get_thd_id();

	if (test_id != c_id) {
		print_vals(6, test_id, c_id);
	}

//	print_vals(3, thd_id, 0);

	cos_upcall(1);
	/* while (1) {
		//cos_switch_thread(thd_id);
		} */
}

int spd2_fn(void)
{
	int bthd;
	unsigned int i, cnt = 0;

	thd_id = cos_get_thd_id();
	curr_thd = thd_id;
	
#ifdef COOPERATIVE
	new_thd = cos_create_thread(thread_tramp, get_stack_addr(1), (void*)1);//MNULL, 0, 0);

	if (new_thd != 1) {
		print_vals(7, new_thd, 1);
	}
#endif
	bthd = cos_brand(0, COS_BRAND_CREATE);
	cos_brand(bthd, COS_BRAND_ADD_THD);

#ifdef PREEMPT
	for (i = 0 ; 1 ; i++) {
		if (i == 0) /* when we wrap-around */
			print_vals(thd_id, cnt++, 10);
	}
#endif

//	curr_thd = new_thd;
//	cos_switch_thread(new_thd);

	/*for (i = 0 ; i < 100000 ; i++) {
		cos_switch_thread(new_thd);
		}*/

//	print_vals(2, thd_id, new_thd);

	//cos_resume_return(thd_id); /* should return with 0 */

	return 1234;//cos_resume_thread(6);
}

void yield(void)
{
	curr_thd = (curr_thd == thd_id) ? new_thd : thd_id;
//	print_vals(curr_thd, 0, 0);
//	cos_resume_return(curr_thd); 
	cos_switch_thread(curr_thd);
	//print_vals(404, 0, 0);
}


void cos_upcall_fn(vaddr_t data_region, int id, 
		   void *arg1, void *arg2, void *arg3)
{
	static int cnt = 0;

//	print_vals(thd_id, cnt++, 9);

	yield();

	return;
}
