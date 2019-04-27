#include <cos_kernel_api.h>
#include <cos_types.h>

#include <pong.h>
#include <ps.h>

#define ITER 1024

void cos_init(void)
{
	int r0 = 0, r1 = 0;
	unsigned long r3 = 0;
	compid_t us, them;
	thdid_t tid;
	int i;
	ps_tsc_t begin, end;

	pong_call();
	assert(pong_ret() == 42);
	assert(pong_arg(1024) == 1024);
	assert(pong_args(1, 2, 3, 4) == 10);
	assert(pong_argsrets(4, 3, 2, 1, &r0, &r1) == 2);
	assert(r0 == 4 && r1 == 3);
	assert(pong_subset(8, 16, &r3) == -24 && r3 == 24);
	tid = pong_ids(&us, &them);
	//assert(cos_thdid() == tid && us != them && us == cos_compid());

	begin = ps_tsc();
	for (i = 0; i < ITER; i++) {
		pong_call();
	}
	end = ps_tsc();
	printc("Fast-path invocation: %llu cycles\n", (end - begin)/ITER);

	begin = ps_tsc();
	for (i = 0; i < ITER; i++) {
		pong_argsrets(0, 0, 0, 0, &r0, &r1);
	}
	end = ps_tsc();
	printc("Three return value invocation: %llu cycles\n", (end - begin)/ITER);

	BUG();
}
