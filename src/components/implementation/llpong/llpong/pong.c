#include <cos_component.h>
#include <cos_debug.h>

#include <stdio.h>
#include <string.h>

#include <pong.h>
#include <ck_pr.h>

unsigned long long tsc_start(void)
{
	unsigned long cycles_high, cycles_low; 
	asm volatile ("movl $0, %%eax\n\t"
		      "CPUID\n\t"
		      "RDTSCP\n\t"
		      "movl %%edx, %0\n\t"
		      "movl %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low) :: 
		      "%eax", "%ebx", "%ecx", "%edx");

	return ((unsigned long long)cycles_high << 32) | cycles_low;
}

int
prints(char *s)
{
	int len = strlen(s);
	cos_print(s, len);
	return len;
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
	return 0;
}

void call(void) { 
	call_cap(0,0,0,0,0);
	return; 
}

char *shmem = (char *)(0x45000000-PAGE_SIZE);

int delay(unsigned long cycles) {
	unsigned long long s,e;
	volatile int mem = 0;

	s = tsc_start();
	while (1) {
		e = tsc_start();
		if (e - s > cycles) return 0; // x us
		mem++;
	}

	return 0;
}

void cos_init(void) {
	int ret;
	int rcv = 0;
	int target = cos_cpuid() - SND_RCV_OFFSET;

	u64_t *pong_shmem = (u64_t *)&shmem[(cos_cpuid()) * CACHE_LINE];
	u64_t e;
	volatile int last_tick, curr_tick;

//	printc(">>> core %d: rcv thd %d in pong, reply target %d\n", cos_cpuid(), cos_get_thd_id(), target);
//#define THD_SWITCH
#ifdef THD_SWITCH
	int thd_cap = SND_THD_CAP_BASE + cos_cpuid()*captbl_idsize(CAP_THD);
	while (1) {
//		printc("PONG: core %ld switching to thd %d\n", cos_cpuid(), thd_cap);
		ret = cap_switch_thd(thd_cap);
		*pong_shmem = tsc_start();
		if (unlikely(ret)) {
			printc("PONG: thd switch failed\n!");
		}
	}
#else

	last_tick = printc("FLUSH!!");
	while (1) {
		ret = call_cap(ACAP_BASE + captbl_idsize(CAP_ARCV)*cos_cpuid(),0,0,0,0);
		e = tsc_start();
		cos_inst_bar();
		curr_tick = printc("FLUSH!!");
		cos_inst_bar();
//		printc("cpu %d in pong\n", cos_cpuid());

		/* rcv++; */
		/* if (rcv % 1024 == 0) printc("core %ld: pong rcv %d ipis!\n", cos_cpuid(), rcv); */

		if (unlikely(curr_tick != last_tick)) {
			delay(10000);
//			printc("PONG cpu %ld, timer detected @ %llu, %d, %d\n", cos_cpuid(), e, last_tick, curr_tick);
			//if (last_tick+1 != curr_tick) printc("PONG tick diff > 1: %u, %u\n", last_tick,curr_tick);
			last_tick = curr_tick;
			*pong_shmem = 1; // ping will drop this measurement
		} else {
//		ck_pr_store_uint(pong_shmem, e);
			*pong_shmem = e;
		}

//		printc("core %ld: rcv thd %d back in pong, ret %d!\n", cos_cpuid(), cos_get_thd_id(), ret);
		/* if (unlikely(ret)) { */
		/* 	printc("ERROR: arcv ret %d", ret); */
		/* 	printc("rcv thd %d switching back to alpha %ld!\n",  */
		/* 	       cos_get_thd_id(), SCHED_CAPTBL_ALPHATHD_BASE + cos_cpuid()*captbl_idsize(CAP_THD)); */
		/* 	ret = cap_switch_thd(SCHED_CAPTBL_ALPHATHD_BASE + cos_cpuid()*captbl_idsize(CAP_THD)); */
		/* } */
		
		/* reply if doing round-trip */
//		assert(target >= 0);
//		ret = call_cap(ACAP_BASE + captbl_idsize(CAP_ASND)*target, 0, 0, 0, 0);
//		printc("core %d replied to target %d, ret %d\n", cos_cpuid(), target, ret);
	}
#endif
	return; 
}
