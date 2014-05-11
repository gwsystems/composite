#include <cos_component.h>
#include <cos_debug.h>

#include <pong.h>

#include <stdio.h>
#include <string.h>

int
prints(char *str)
{
	/* int left; */
	/* char *off; */
	/* const int maxsend = sizeof(int) * 3; */

	/* if (!str) return -1; */
	/* for (left = cos_strlen(str), off = str ;  */
	/*      left > 0 ;  */
	/*      left -= maxsend, off += maxsend) { */
	/* 	int *args; */
	/* 	int l = left < maxsend ? left : maxsend; */
	/* 	char tmp[maxsend]; */

	/* 	cos_memcpy(tmp, off, l); */
	/* 	args = (int*)tmp; */
	/* 	print_char(l, args[0], args[1], args[2]); */
	/* }  */
	return 0;
}

int __attribute__((format(printf,1,2))) 
printc(char *fmt, ...)
{
	char s[128];
	va_list arg_ptr;
	int ret, len = 128;

	va_start(arg_ptr, fmt);
	ret = vsnprintf(s, len, fmt, arg_ptr);
	va_end(arg_ptr);
	cos_print(s, ret);

	return ret;
}

/* void delay(void) */
/* { */
/* 	int i,j,k; */
/* 	volatile int m = 0; */

/* 	for (i = 0; i < 1000; i++) */
/* 		for(j = 0; j < 100; j++) */
/* 			for (k = 0; k < 100; k++) */
/* 				m = 123; */
/* } */

/* void test_fn(int *data) { */
/* 	printc("Working fn: core %ld, thd %d (thd num %d), data0 %d\n", */
/* 	       cos_cpuid(), cos_get_thd_id(), ainv_get_thd_num(), *(data + 4*ainv_get_thd_num())); */
	
/* 	return; */
/* } */

#define ITER (1024)
//u64_t meas[ITER];

void pingpong(void)
{
	int i;
	u64_t s, e;

	printc("core %ld: doing pingpong\n", cos_cpuid());
	
	call_cap(2, 0, 0, 0, 0);

	rdtscll(s);
	for (i = 0; i < ITER; i++) {
		call_cap(2, 0, 0, 0, 0);
	}
	rdtscll(e);

	printc("core %ld: pingpong done, avg %llu\n", cos_cpuid(), (e-s)/ITER);

	return;
}

volatile int arcv_ready[NUM_CPU];
#define SPINTIME (1000000000L)

void cos_init(void)
{
	int i;
	u64_t s, e;

	//init rcv thd first.
	if (cos_cpuid() == INIT_CORE) {
		while (arcv_ready[1] == 0) ;
		printc("core %ld: sending ipi\n", cos_cpuid());
		rdtscll(s);
		for (i = 0; i<ITER; i++) {
			call_cap(ACAP_BASE + captbl_idsize(CAP_ASND)*1, 0, 0, 0, 0);
//			printc("core %ld: ipi ping delay\n", cos_cpuid());
//			printc("core %ld: ipi ping delay2\n", cos_cpuid());
		}
		rdtscll(e);
		printc("core %ld: ipi done, avg %llu\n", cos_cpuid(), (e-s)/ITER);
	} else if (cos_cpuid() == 1){
		printc("core %ld: thd %d switching to pong thd\n", cos_cpuid(), cos_get_thd_id());
		cap_switch_thd(RCV_THD_CAP_BASE + captbl_idsize(CAP_THD)*cos_cpuid());
		printc("core %ld: thd %d back in ping\n", cos_cpuid(), cos_get_thd_id());

		arcv_ready[cos_cpuid()] = 1;
		rdtscll(s);
		while (1) {
			rdtscll(e);
			if ((e-s) > SPINTIME) break;
		}
		printc("core %ld: exiting from ping\n", cos_cpuid());
	} else {
		pingpong();
	}

	cap_switch_thd(SCHED_CAPTBL_ALPHATHD_BASE + cos_cpuid());

	call();

	return;
	/* u64_t start, end, avg, tot = 0, dev = 0; */
	/* int i, j; */


/* 	printc("cpu %ld, thd %d from ping\n",cos_cpuid(), cos_get_thd_id()); */
/* //	call(111,222,333,444);			/\* get stack *\/ */

/* 	printc("core %ld: spinning....\n", cos_cpuid()); */
/* 	delay(); */
/* 	printc("core %ld: after spin!\n", cos_cpuid()); */
	
/* //	call(1111,2222,3333,4444);			/\* get stack *\/ */
/* 	printc("Starting %d Invocations.\n", ITER); */
	
/* 	int params[8] = {1,2,3,4, 11, 22, 33, 44}; */
/* 	par_inv(test_fn, (void *)params, 2); */

/* 	return; */

/* 	for (i = 0 ; i < ITER ; i++) { */
/* 		rdtscll(start); */
/* //		cos_send_ipi(i, 0, 0, 0); */
/* 		call(1,2,3,4); */
/* 		rdtscll(end); */
/* 		meas[i] = end-start; */
/* 	} */

/* 	for (i = 0 ; i < ITER ; i++) tot += meas[i]; */
/* 	avg = tot/ITER; */
/* 	printc("avg %lld\n", avg); */
/* 	for (tot = 0, i = 0, j = 0 ; i < ITER ; i++) { */
/* 		if (meas[i] < avg*2) { */
/* 			tot += meas[i]; */
/* 			j++; */
/* 		} */
/* 	} */
/* 	printc("avg w/o %d outliers %lld\n", ITER-j, tot/j); */

/* 	for (i = 0 ; i < ITER ; i++) { */
/* 		u64_t diff = (meas[i] > avg) ?  */
/* 			meas[i] - avg :  */
/* 			avg - meas[i]; */
/* 		dev += (diff*diff); */
/* 	} */
/* 	dev /= ITER; */
/* 	printc("deviation^2 = %lld\n", dev); */

/* 	printc("last invocation...\n"); */
/* 	rdtscll(start); */
/* 	int rrr = call(11,22,33,44); */
/* 	rdtscll(end); */
/* 	printc("done ret %d. cost %llu \n", rrr, end-start); */

/* 	rdtscll(start); */
/* 	rrr = call(11,22,33,44); */
/* 	rdtscll(end); */
/* 	printc("done ret %d. cost %llu \n", rrr, end-start); */

/* //	printc("%d invocations took %lld\n", ITER, end-start); */
/* 	return; */
}
