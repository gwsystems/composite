#include <cos_component.h>
#include <cos_debug.h>

#include <stdio.h>
#include <string.h>

#include <pong.h>
#include <ck_pr.h>

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

char *shmem = (char *)(0x45000000-PAGE_SIZE);

void cos_init(void) {
	int ret;
	int rcv = 0;
	int target = cos_cpuid() - SND_RCV_OFFSET;
	assert(target >= 0);

//	printc("core %d: rcv thd %d in pong, reply target %d\n", cos_cpuid(), cos_get_thd_id(), target);
	u64_t *pong_shmem = (u64_t *)&shmem[(cos_cpuid()) * CACHE_LINE];
	u64_t e;
	int last_tick, curr_tick;

	last_tick = printc("FLUSH!!");
	while (1) {
		ret = call_cap(ACAP_BASE + captbl_idsize(CAP_ARCV)*cos_cpuid(),0,0,0,0);

		rdtscll(e);
		curr_tick = printc("FLUSH!!");
		if (unlikely(curr_tick != last_tick)) {
//			printc("timer detected @ %llu, %d, %d, (cost %llu)\n", e, last_tick, curr_tick, e-s);
//			if (last_tick+1 != curr_tick) printc("PONG tick diff > 1: %u, %u\n", last_tick,curr_tick);
			last_tick = curr_tick;
			*pong_shmem = 1; // ping will drop this measurement
		} else {
//		ck_pr_store_uint(pong_shmem, e);
			*pong_shmem = e;
		}

//		printc("core %ld: rcv thd %d back in pong, ret %d!\n", cos_cpuid(), cos_get_thd_id(), ret);
		if (unlikely(ret)) {
			printc("ERROR: arcv ret %d", ret);
			printc("rcv thd %d switching back to alpha %ld!\n", 
			       cos_get_thd_id(), SCHED_CAPTBL_ALPHATHD_BASE + cos_cpuid()*captbl_idsize(CAP_THD));
			ret = cap_switch_thd(SCHED_CAPTBL_ALPHATHD_BASE + cos_cpuid()*captbl_idsize(CAP_THD));
		}
		rcv++;
//		if (rcv % 1024 == 0) printc("core %ld: pong rcv %d ipis!\n", cos_cpuid(), rcv);
		
		/* reply if doing round-trip */
//		ret = call_cap(ACAP_BASE + captbl_idsize(CAP_ASND)*target, 0, 0, 0, 0);

//		printc("core %d replied to target %d, ret %d\n", cos_cpuid(), target, ret);
	}

	return; 
}
