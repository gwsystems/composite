#include "aed_ioctl.h"

#include <stdio.h>

#define rdtscll(val) \
      __asm__ __volatile__("rdtsc" : "=A" (val))

#define ITERATIONS 10000
//#define MEAS

void print(void)
{
	printf("(*)\n");
	fflush(stdout);
	return;
}
int print_val(unsigned int v)
{
	printf("cap: %x.\n", v);

	return 6;
}

extern void invokee(void);
extern void cos_intermediate(void);
extern int intermed_cap;

extern unsigned int SS_ipc_client_marshal(int cap) __attribute__ ((regparm(1)));
extern void SS_ipc_server_unmarshal(void);

int c_invokee(void)
{
//	printf("In invoked fn!\n");
	return 6;
}

int invoker(int cap)
{
	int ret;

	cap = cap<<16;
#ifndef MEAS
	printf("making invocation on cap %d.\n", cap);
#endif
	__asm__ __volatile__ ( "pushl %%ebp\n\t"
			       "pushl %%ebx\n\t" 
			       "pushl %%ecx\n\t" 
			       "pushl %%edx\n\t" 
			       "movl %%esp, %%ebp\n\t" 
			       "movl $after_inv, %%ecx\n\t" 
			       "movl %1, %%eax\n\t" 
			       "sysenter\n" 
//			       "pushl %%eax\n\t"
//			       "call print_val\n\t"
//			       "addl $4, %%esp\n\t"
			       "after_inv:\n\t" 
			       "popl %%edx\n\t" 
			       "popl %%ecx\n\t" 
			       "popl %%ebx\n\t" 
			       "popl %%ebp\n\t" 
			       "movl %%eax, %0\n\t"
			       : "=m" (ret) : "r" (cap));
	return ret;
}

int ucap_tbl1[64];
int ucap_tbl2[64];
int ucap_tbl3[64];

int main(void)
{
	struct spd_info spd1, spd2, spd3;
	struct cos_thread_info thd;
	struct cap_info cap1, cap2;
	int cntl_fd;
	
	cntl_fd = aed_open_cntl_fd();

	spd1.num_caps = 2;
	spd1.ucap_tbl = (vaddr_t)ucap_tbl1;
	spd2.num_caps = 1;
	spd2.ucap_tbl = (vaddr_t)ucap_tbl2;
	spd3.num_caps = 0;
	spd3.ucap_tbl = (vaddr_t)ucap_tbl3;

	spd1.spd_handle = cos_create_spd(cntl_fd, &spd1);
	printf("spd 1: %d.\n", (unsigned int)spd1.spd_handle);
	spd2.spd_handle = cos_create_spd(cntl_fd, &spd2);
	printf("spd 2: %d.\n", (unsigned int)spd2.spd_handle);
	spd3.spd_handle = cos_create_spd(cntl_fd, &spd3);
	printf("spd 3: %d.\n", (unsigned int)spd3.spd_handle);

	cap1.ST_serv_entry = (vaddr_t)cos_intermediate;//invokee;
	cap1.owner_spd_handle = spd1.spd_handle;
	cap1.dest_spd_handle = spd2.spd_handle;
	cap1.il = 0;
	cap1.cap_handle = cos_spd_add_cap(cntl_fd, &cap1);
 	if (cap2.cap_handle == 0) {
		printf("Could not add capability to spd %d.\n", cap2.owner_spd_handle);
		return -1;
	}

	cap2.ST_serv_entry = (vaddr_t)SS_ipc_server_unmarshal;//invokee;
	cap2.owner_spd_handle = spd1.spd_handle;//spd2.spd_handle;
	cap2.dest_spd_handle = spd3.spd_handle;
	cap2.il = 0;
	cap2.cap_handle = cos_spd_add_cap(cntl_fd, &cap2);
 	if (cap2.cap_handle == 0) {
		printf("Could not add capability to spd %d.\n", cap2.owner_spd_handle);
		return -1;
	}
	intermed_cap = cap2.cap_handle;

	printf("Test created cap %d and cap %d.\n", (unsigned int)cap1.cap_handle, (unsigned int)cap2.cap_handle);

	thd.spd_handle = spd1.spd_handle;
	cos_create_thd(cntl_fd, &thd);

	printf("OK, good to go, calling fn\n");
	fflush(stdout);

#ifdef MEAS
	{
		unsigned long long end, start, total;
		int i;

		rdtscll(start);
		for (i = 0 ; i < ITERATIONS ; i++) {
			SS_ipc_client_marshal(cap2.cap_handle<<16); //invoker(cap1.cap_handle);
		}
		rdtscll(end);
		total = (double)(end-start)/ITERATIONS;
		printf("time %lld, start: %lld, end: %lld\n", total, start, end);
	}
#endif

	printf("Returned %x.\n", /* invoker(cap1.cap_handle)); */SS_ipc_client_marshal(cap2.cap_handle<<16));
	
	return 0;
}
