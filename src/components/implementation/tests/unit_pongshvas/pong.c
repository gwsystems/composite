#include <cos_types.h>
#include <pongshvas.h>
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
unsigned long shared = 42;

void
cos_init(void)
{
	assert(state == PONG_UNINIT);
	printc("Pong Shared VAS component %ld: cos_init\n", cos_compid());
	state = PONG_INIT;
}

void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	assert(state == PONG_INIT);
	simple_barrier(&init_barrier);
	if (init_core) {
		printc("Pong Shared VAS component %ld: cos_parallel_init on core %d (of %d)\n", cos_compid(), cid, ncores);
		initcore = cid;
	}

	state = PONG_PARINIT;
}

void
parallel_main(coreid_t cid)
{
	assert(state == PONG_PARINIT);
	simple_barrier(&main_barrier);
	if (cos_coreid() == initcore) printc("Pong Shared VAS component %ld: parallel main\n", cos_compid());
	state = PONG_PARMAIN;
}

unsigned long *
pongshvas_send(void) 
{
	printc("IN PONG\n");
	return &shared;
}

void
pongshvas_rcv_and_update(unsigned long *shared)
{
	*shared += 10;
}