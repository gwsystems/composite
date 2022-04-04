#include <cos_component.h>
#include <cos_kernel_api.h>
#include <llprint.h>
#include <pong.h>
#include <cos_debug.h>
#include <cos_types.h>
#include <barrier.h>

/* Test the initialization order, and its relationship to ping */
typedef enum {
	PONG_UNINIT,
	PONG_INIT,
	PONG_PARINIT,
	PONG_PARMAIN
} pong_init_state_t;

volatile pong_init_state_t state = PONG_UNINIT;
struct simple_barrier init_barrier = SIMPLE_BARRIER_INITVAL;
volatile coreid_t initcore;
struct simple_barrier main_barrier = SIMPLE_BARRIER_INITVAL;

void
cos_init(void)
{
	assert(state == PONG_UNINIT);
	printc("Pong component %ld: cos_init\n", cos_compid());
	state = PONG_INIT;
}

void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	assert(state == PONG_INIT);
	simple_barrier(&init_barrier);
	if (init_core) {
		printc("Pong component %ld: cos_parallel_init on core %d (of %d)\n", cos_compid(), cid, ncores);
		initcore = cid;
	}

	state = PONG_PARINIT;
}

void
parallel_main(coreid_t cid)
{
	assert(state == PONG_PARINIT);
	simple_barrier(&main_barrier);
	if (cos_coreid() == initcore) printc("Pong component %ld: parallel main\n", cos_compid());
	state = PONG_PARMAIN;
}

/* We assume that ping will call pong_call upon initialization. */
void
pong_call(void)
{
	assert(state >= PONG_PARINIT);

	return;
}

int
pong_ret(void)
{
	return 42;
}

int
pong_arg(int p1)
{
	return p1;
}

int
pong_args(int p1, int p2, int p3, int p4)
{
	return p1 + p2 + p3 + p4;
}

int
pong_wideargs(long long p0, long long p1)
{
	if (p0 <= ((long long)1 << 31) && p1 <= ((long long)1 << 31)) return p0 + p1;

	return p0 < p1 ? -1 : (p0 == p1 ? 0 : 1);
}

int
pong_argsrets(int p0, int p1, int p2, int p3, word_t *r0, word_t *r1)
{
	*r0 = p0;
	*r1 = p1;

	return p2 + p3;
}

long long
pong_widerets(long long p0, long long p1)
{
	return p0 + p1;
}

int
pong_subset(unsigned long p0, unsigned long p1, unsigned long *r0)
{
	*r0 = p0 + p1;
	return -p0 - p1;
}

thdid_t
pong_ids(compid_t *client, compid_t *serv)
{
	*client = (compid_t)cos_inv_token();
	*serv   = cos_compid();

	return cos_thdid();
}
