#include "out.h"
#include <stdio.h>
extern int blah(void);

#define ITER 10000
#define rdtscll(val) \
      __asm__ __volatile__("rdtsc" : "=A" (val))

int foo(void) 
{
//	printf("foo\n");
	return 2;
}

void bar(void)
{
	int iter;
	unsigned long long start, end;

	printf("bar\n");
	printf("foo (%x) ret: %x\n", (unsigned int)foo, blah());

	rdtscll(start);
	for (iter = ITER; iter > 0 ; iter--) {
		blah();
	}
	rdtscll(end);
	
	printf("ST call takes %lld.\n", (end-start)/ITER);

}

extern struct usr_inv_cap ST_user_caps[];
extern void ST_direct_invocation(void);

void set_cap(struct usr_inv_cap *cap, vaddr_t inv_fn, vaddr_t entry_fn,
	     unsigned int inv_cnt, unsigned int cap_no)
{
	cap->invocation_fn = inv_fn;
	cap->service_entry_inst = entry_fn;
	cap->invocation_count = inv_cnt;
	cap->cap_no = cap_no;

	return;
}

int main(void)
{
	set_cap(&ST_user_caps[0], 0, 0, 0, 0);
	set_cap(&ST_user_caps[1], (vaddr_t)ST_direct_invocation, (vaddr_t)foo, 0, 61);
	
	printf("cap layout [0]: %x, [1]: %x; entry size %d\n", (unsigned int)&ST_user_caps[0],
	       (unsigned int)&ST_user_caps[1], (unsigned int)&ST_user_caps[1] - (unsigned int)&ST_user_caps[0]);
	printf("ST_direct_invocation: %x\n", (unsigned int)ST_direct_invocation);
	bar();

	printf("%d\n", ST_user_caps[1].invocation_count);
}
