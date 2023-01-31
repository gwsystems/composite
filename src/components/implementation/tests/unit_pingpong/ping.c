#include <cos_kernel_api.h>
#include <cos_types.h>
#include <pong.h>
#include <ps.h>

#define ITER 1024

volatile ps_tsc_t fast_path, all_args;

void
cos_init(void)
{
	word_t r0 = 0, r1 = 0;
	unsigned long r3 = 0;
	compid_t us, them;
	thdid_t tid;
	int i;
	ps_tsc_t begin, end;
	long long a = (long long)3 << 32 | (long long)1;
	long long b = (long long)4 << 32 | (long long)2;
	int ret;
	long long ret_ll;

	printc("Ping component %ld: cos_init execution\n", cos_compid());

	pong_call();
	ret = pong_ret();
	assert(ret == 42);
	ret = pong_arg(1024);
	assert(ret == 1024);
	ret = pong_args(1, 2, 3, 4);
	assert(ret == 10);
	ret = pong_argsrets(4, 3, 2, 1, &r0, &r1);
	assert(ret == 3);
	assert(r0 == 4 && r1 == 3);
	ret = pong_wideargs(a, b);
	assert(ret == -1);
	ret_ll = pong_widerets(a, b);
	assert(ret_ll == (a + b));
	a = 1;
	b = 2;
	ret = pong_wideargs(a, b);
	assert(ret == (int)(a + b));
	ret_ll = pong_widerets(a, b);
	assert(ret_ll == (a + b));
	ret = pong_subset(8, 16, &r3);
	assert(ret == -24 && r3 == 24);
	tid = pong_ids(&us, &them);
	assert(cos_thdid() == tid && us != them && us == cos_compid());

	begin = ps_tsc();
	for (i = 0; i < ITER; i++) {
		pong_call();
	}
	end = ps_tsc();
	fast_path = (end - begin)/ITER;

	begin = ps_tsc();
	for (i = 0; i < ITER; i++) {
		pong_argsrets(0, 0, 0, 0, &r0, &r1);
	}
	end = ps_tsc();
	all_args = (end - begin)/ITER;

	return;
}

int
main(void)
{
	printc("Ping component %ld: main execution\n", cos_compid());
	printc("Fast-path invocation: %llu cycles\n", fast_path);
	printc("Three return value invocation: %llu cycles\n", all_args);

	return 0;
}
