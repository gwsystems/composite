#include <cos_kernel_api.h>
#include <cos_types.h>
#include <ps.h>

static word_t counter_cos_parallel = 0;
static word_t counter_parallel_main = 0;

void
cos_init(void)
{
	printc("cos_init, init core id :%ld\n",cos_cpuid());
	return;
}

void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	ps_faa(&counter_cos_parallel, 1);

	return;
}

void
parallel_main(coreid_t cid, int init_core, int ncores)
{
	ps_faa(&counter_parallel_main, 1);

	while (ps_load(&counter_parallel_main) < (word_t)ncores);

	assert(counter_cos_parallel == (word_t)ncores && counter_parallel_main == (word_t)ncores);

	if(init_core) {
		printc("Multi processor test, total core num %d: SUCCESS\n",ncores);
	}

	return;
}

int
main(void)
{
	/* It should never come here*/
	assert(0);
	return 0;
}
