#include <cos_kernel_api.h>
#include <cos_types.h>
#include <pongshvas.h>

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
	unsigned long rcv;

	printc("Shared VAS Ping component %ld: cos_init execution\n", cos_compid());

	/* this doesnt return a pointer anymore because ping cannot
	 * access pong's memory even though they are in the same VAS
	 * because of MPK
	 */
	begin = ps_tsc();
	for (i = 0; i < ITER; i++) {
		rcv = pongshvas_send();
		assert(rcv == 42);
	}
	end = ps_tsc();
	printc("UL Invocation: %llu cycles\n", (end-begin)/ITER);
	
	/* this is left commented out bc in the current booter format client and server aren't in shared VAS (so rcv_and_update would crash)
	 * pongshvas_rcv_and_update(shared);
	 * assert(*shared == 52);
	 */
	return;
}

int
main(void)
{
	printc("Shared VAS Ping component %ld: main execution\n", cos_compid());

	return 0;
}
