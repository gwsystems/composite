#include <cos_component.h>
#include <print.h>

#include <periodic_wake.h>
#include <sched.h>
#include <timed_blk.h>
#include <cbuf_mid.h>

#define PERIODIC 100

void cos_init(void)
{

	printc("\n****** TOP: thread %d in spd %ld ******\n",cos_get_thd_id(), cos_spd_id());

	periodic_wake_create(cos_spd_id(), PERIODIC);

	int k = 0;
	int th;
	int i = 0;
	
	/* do{ */
	/* 	periodic_wake_wait(cos_spd_id()); */
	/* }while (i++ < waiting); */
	i = 0;

	/* printc("thd %d start testing...!\n", cos_get_thd_id()); */

	while(1){
		k++;
		/* printc("kkkkkk %d\n",k); */
		th = cos_get_thd_id();

		/* if ( (th == 16 || th == 17) && k >= 2)  */
		/* 	break; */

		cbuf_call('a');
		/* if(cos_get_thd_id() == 15) while(1); */
		periodic_wake_wait(cos_spd_id());
	}

	return;

}
