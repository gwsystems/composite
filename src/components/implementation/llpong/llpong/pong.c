#include <cos_component.h>
#include <cos_debug.h>

#include <stdio.h>
#include <string.h>

#include <pong.h>

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

void call(void) { 
	call_cap(0,0,0,0,0);
	return; 
}

void cos_init(void) {
	int ret;
	int rcv = 0;
	printc("rcv thd %d in pong\n", cos_get_thd_id());

	while (1) {
		ret = call_cap(ACAP_BASE + captbl_idsize(CAP_ARCV)*cos_cpuid(),0,0,0,0);
//		printc("core %ld: rcv thd %d back in pong, ret %d!\n", cos_cpuid(), cos_get_thd_id(), ret);
		if (ret) {
			printc("ERROR: arcv ret %d", ret);
			printc("rcv thd %d switching back to alpha %d!\n", 
			       cos_get_thd_id(), SCHED_CAPTBL_ALPHATHD_BASE + cos_cpuid());
			ret = cap_switch_thd(SCHED_CAPTBL_ALPHATHD_BASE + cos_cpuid());
		}
		rcv++;
		if (rcv % 1024 == 0) printc("core %ld: pong rcv %d ipis!\n", cos_cpuid(), rcv);
	}

	return; 
}
