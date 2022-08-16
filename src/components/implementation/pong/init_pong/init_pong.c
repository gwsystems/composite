/**
 * Each client in tests/unit_init invokes pong_arg for each of
 * cos_init, cos_parallel_init, and parallel_main. This enables us to
 * assess execution progress across clients and this component. The
 * invariants we're looking to check include:
 *
 * 1. Servers execute all of the initialization functions before
 *    clients. This effectively means, this components initialization
 *    proceeds before the unit_inits.
 * 2. Initialization is non-preemptive, so a component that begins
 *    initialization will finish it before moving on to the next
 *    component.
 * 3. Each component's initialization procedures progress in order:
 *    cos_init, then cos_parallel_init, then parallel_main.
 * 4. All mains of all components should execute.
 */

#include <cos_component.h>
#include <util.h>
#include <pong.h>

int cos_init_executed          = 0;
int cos_parallel_init_executed = 0;
int parallel_main_executed     = 0;

/*
 * Invariant #3: We're checking that our own initialization proceeds
 * in order.
 */
void
parallel_main(coreid_t cid)
{
	assert(cos_init_executed == 1);
	assert(cos_parallel_init_executed == NUM_CPU);
	assert(parallel_main_executed < NUM_CPU);

	cos_faa(&parallel_main_executed, 1);
}

void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	assert(cos_parallel_init_executed < NUM_CPU);
	assert(parallel_main_executed == 0);
	assert(cos_init_executed == 1);
	assert(ncores == NUM_CPU);

	cos_faa(&cos_parallel_init_executed, 1);
}

void
cos_init(void)
{
	assert(cos_parallel_init_executed == 0);
	assert(cos_init_executed == 0);
	assert(parallel_main_executed == 0);

	cos_faa(&cos_init_executed, 1);
}

#define MAX_CLIENTS 8

typedef enum {
	STAGE_INIT,
	STAGE_PARINIT,
	STAGE_MAIN,
	STAGE_MAX
} stage_t;

struct client_state {
	unsigned long client;
	int stage_cnt[STAGE_MAX];
} clients[MAX_CLIENTS];

static int
validate(unsigned long client, stage_t stage)
{
	unsigned int i, j;
	int found = 0;
	unsigned int offset;

	assert(stage < STAGE_MAX);

	/* Where is this client? */
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (clients[i].client == client) {
			offset = i;
			found = 1;
		}
	}
	assert(found);

	/* Lets just make sure that all previous stages are done. */
	for (j = 0; j < stage; j++) {
		int expected = (j == STAGE_INIT) ? 1 : NUM_CPU;

		assert(clients[offset].stage_cnt[j] == expected);
	}

	/*
	 * Invariant #2: If this client is initializing, all previous
	 * should have initialized.
	 */
	if (stage == STAGE_INIT || stage == STAGE_PARINIT) {
		for (i = 0; i < offset; i++) {
			assert(clients[i].stage_cnt[STAGE_INIT] == 1);
			assert(clients[i].stage_cnt[STAGE_PARINIT] == NUM_CPU);
		}
	}

	return 0;
}

static void
assess_success(void)
{
	int i;

	for (i = 0; clients[i].client > 0; i++) {
		if (clients[i].stage_cnt[STAGE_MAIN] < NUM_CPU) return;
	}
	/*
	 * Invariant #4: have all clients achieved main execution?
	 */
	printc("SUCCESS: execution ordering of component initialization.\n");
}

int
pong_arg(int stage)
{
	int i;
	unsigned long client = (unsigned long)cos_inv_token();

	/* Invariant #1: servers should execute before clients! */
	assert(cos_init_executed == 1);
	assert(cos_parallel_init_executed == NUM_CPU);
	assert(stage < STAGE_MAX);

	/* Update our tracking of the client's state */
	for (i = 0; i < MAX_CLIENTS; i++) {
		cos_cas(&clients[i].client, 0, client);
		if (clients[i].client == client) {
			cos_faa(&clients[i].stage_cnt[stage], 1);

			break;
		}
	}
	assert(i < MAX_CLIENTS);
	if (validate(client, stage)) BUG();

	assess_success();

	return 0;
}

void pong_call(void) { BUG(); }
int pong_ret(void) { BUG(); return 0; }
int pong_args(int p1, int p2, int p3, int p4) { BUG(); return 0; }
int pong_wideargs(long long p0, long long p1) { BUG(); return 0; }
int pong_argsrets(int p0, int p1, int p2, int p3, word_t *r0, word_t *r1) { BUG(); return 0; }
long long pong_widerets(long long p0, long long p1) { BUG(); return 0; }
int pong_subset(unsigned long p0, unsigned long p1, unsigned long *r0) { BUG(); return 0; }
thdid_t pong_ids(compid_t *client, compid_t *serv) { BUG(); return 0; }
