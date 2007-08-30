#include <cos_component.h>

extern int print_vals(int a, int b, int c);

int spd2_fn(void)
{
	int thd_id, new_thd = 0;

	thd_id = cos_get_thd_id();
	
	print_vals(1, 0, 0);

	new_thd = cos_create_thread();//MNULL, 0, 0);

	print_vals(1, thd_id, new_thd);
	//cos_switch_thread(thd_id);

	cos_resume_return(thd_id); /* should return with 0 */

	return 1234;//cos_resume_thread(6);
}
