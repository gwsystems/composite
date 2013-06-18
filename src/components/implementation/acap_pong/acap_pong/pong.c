#include <cos_component.h>
#include <print.h>
#include <acap_pong.h>

#include <acap_mgr.h>

//volatile int f;
//void call(void) { f = *(int*)NULL; return; }
int call(int a, int b, int c, int d) { return a+b+c+d; }




/// move to lib later
struct __cos_ainv_srv_thd {
	
	volatile int stop;
} CACHE_ALIGNED;

struct __cos_ainv_srv_thd *__cos_ainv_thds[MAX_NUM_THREADS]; 

int cos_ainv_handling(void) {
	int curr = cos_get_thd_id();
	int acap = acap_srv_lookup(cos_spd_id());

	while (curr == 0) {
		//ainv_wait(acap);
		// check requests in ring buf
	}

	return 0;
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_AINV_HANDLER:
	{
		cos_ainv_handling();
		
		break;
	}
	/* case COS_UPCALL_BRAND_EXEC: */
	/* { */
	/* 	cos_upcall_exec(arg1); */
	/* 	break; */
	/* } */
	/* case COS_UPCALL_BOOTSTRAP: */
	/* { */
	/* 	static int first = 1; */
	/* 	if (first) {  */
	/* 		first = 0;  */
	/* 		__alloc_libc_initilize();  */
	/* 		constructors_execute(); */
	/* 	} */
	/* 	cos_init(arg1); */
	/* 	break; */
	/* } */
	default:
		/* fault! */
		*(int*)NULL = 0;
		return;
	}
	return;
}
