#include <cos_component.h>
#include <pong.h>

int cos_init_executed          = 0;
int cos_parallel_init_executed = 0;
int parallel_main_executed     = 0;

enum {
	STAGE_INIT,
	STAGE_PARINIT,
	STAGE_MAIN,
};

void
parallel_main(coreid_t cid)
{
	assert(cos_init_executed == 1);
	assert(cos_parallel_init_executed == NUM_CPU);
	assert(parallel_main_executed < NUM_CPU);

	cos_faa(&parallel_main_executed, 1);
	pong_arg(STAGE_MAIN);
}

void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	assert(cos_parallel_init_executed < NUM_CPU);
	assert(parallel_main_executed == 0);
	assert(cos_init_executed == 1);
	assert(ncores == NUM_CPU);

	cos_faa(&cos_parallel_init_executed, 1);
	pong_arg(STAGE_PARINIT);
}

void
cos_init(void)
{
	assert(cos_parallel_init_executed == 0);
	assert(cos_init_executed == 0);
	assert(parallel_main_executed == 0);

	cos_faa(&cos_init_executed, 1);
	pong_arg(STAGE_INIT);
}
