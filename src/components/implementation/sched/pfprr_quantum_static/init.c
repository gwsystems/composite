/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu & Gabe Parmer, gparmer@gwu.edu
 */

/**
 * Initialization of the scheduler, and all of the components the
 * scheduler is responsible for initializing. Note that the `init`
 * interface is what encodes which components are dependent on the
 * scheduler for initialization. If the scheduler is managed by the
 * capmgr, then the capmgr must have access to the captbl/pgtbl/comp
 * capabilities for the component, thus it must also depend on the
 * capmgr for the capmgr_create interface.
 *
 * The current implementation is not ideal in multiple ways.
 *
 * 1. It does not provide parallel execution.
 * 2. It executes the initialization of the client components during
 *    the *main* execution of this scheduler. That means that
 *    initialization does *not* compose correctly. For example,
 *    consider a component, `a`, that relies on another initialization
 *    component, and also depends on a component, `b`, that depends on
 *    us for initialization. We will finish initialization, and allow
 *    initialization to proceed in `a`, *before* we actually
 *    initialize `b`. This breaks the initialization ordering
 *    requirements. It doesn't make much sense to have such
 *    cross-scheduler dependencies, so this is less of an issue than
 *    it seems.
 *
 * The former is simple because it is TBD. The latter is because the
 * scheduler loop executes in `main`, and we orchestrate
 * initialization within the normal scheduling loop. To fix this, we'd
 * have to move a version of the scheduling loop (that returns
 * sporadically) to `parallel_init`.
 */

#include <slm.h>
#include <barrier.h>
#include <init.h>
#include <initargs.h>

typedef enum {
	SCHEDINIT_FREE,
	SCHEDINIT_INITING,
	SCHEDINIT_PARINIT,
	SCHEDINIT_MAIN,	/* main, or parallel_main depending on main_type */
} schedinit_t;

struct schedinit_status {
	struct simple_barrier barrier;
	schedinit_t status;
	init_main_t  main_type;
};

static struct schedinit_status initialization_state[MAX_NUM_COMPS] = { 0 };
/* This schedule is used by the initializer_thd to ascertain order */
static unsigned long init_schedule_off = 0;
static compid_t init_schedule[MAX_NUM_COMPS] = { 0 };

static void
component_initialize_next(compid_t cid)
{
	struct schedinit_status *s;

	printc("\tSched %ld: %ld is the %ldth component to initialize\n", cos_compid(), cid, init_schedule_off);
	init_schedule[init_schedule_off] = cid;
	init_schedule_off++;
	s = &initialization_state[cid];
	assert(s->status == SCHEDINIT_FREE);
	*s = (struct schedinit_status) {
		.status = SCHEDINIT_INITING,
		.main_type = INIT_MAIN_NONE,
	};
	simple_barrier(&s->barrier);

	return;
}

static void
calculate_initialization_schedule(void)
{
	struct initargs exec_entries, curr;
	struct initargs_iter i;
	int ret, cont;

	ret = args_get_entry("execute", &exec_entries);
	assert(!ret);
	printc("\tSched %ld: %d components that need execution\n", cos_compid(), args_len(&exec_entries));
	for (cont = args_iter(&exec_entries, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		int      keylen;
		compid_t id        = atoi(args_key(&curr, &keylen));
		char    *exec_type = args_value(&curr);

		assert(exec_type);
		assert(id != cos_compid());

		/* Only init threads allowed */
		if (strcmp(exec_type, "init")) BUG();	/* TODO: no support for hierarchical scheduling yet */

		component_initialize_next(id);
	}

	return;
}

struct slm_thd *slm_current_extern(void);

void
init_done(int parallel_init, init_main_t cont)
{
	compid_t client = (compid_t)cos_inv_token();
	struct schedinit_status *s;

	s = &initialization_state[client];
	assert(s->status != SCHEDINIT_FREE);

	/* Currently we don't do parallel initialization */
	if (parallel_init) {
		ps_store(&s->status, SCHEDINIT_PARINIT);

		return;
	}

	switch (cont) {
	case INIT_MAIN_SINGLE:
		ps_store(&s->main_type, INIT_MAIN_SINGLE);
		break;
	case INIT_MAIN_PARALLEL:
		ps_store(&s->main_type, INIT_MAIN_PARALLEL);
		break;
	case INIT_MAIN_NONE:
		break;
	}

	/* This is the sync value for the initialization thread */
	ps_store(&s->status, SCHEDINIT_MAIN);
	if (s->main_type != INIT_MAIN_NONE) return; /* FIXME: no parallel main currently */

	if (cont == INIT_MAIN_NONE) {
		printc("\tScheduler %ld: Exiting thread %ld from component %ld\n", cos_compid(), cos_thdid(), client);
		slm_thd_deinit(slm_current_extern());	/* No main? No more execution! */
		BUG();
	}

	/* TODO: parallel init and main */

	return;
}

void
init_exit(int retval)
{
	compid_t client = (compid_t)cos_inv_token();

	printc("\tScheduler %ld: Exiting thread %ld from component %ld\n", cos_compid(), cos_thdid(), client);
	slm_thd_deinit(slm_current_extern());
	BUG();
	while (1) ;
}

/**
 * A new thread, at the highest priority executes this function. This
 * thread collaborates with
 * 1. sched_childinfo_init_intern which parses through the components
 *    we are supposed to initialize, and
 * 2. the init_* interface that captures the initialization status of
 *    component's threads
 * to orchestrate the initialization of all components we're
 * responsible for scheduling.
 *
 * *Assumptions and simplifications:* We make a number of
 * simplifications here. Currently, we don't support parallel
 * initialization. We also assume that the initialization thread can
 * either be executed at the highest priority (which might be
 * difficult with some scheduling policies), or will at least not
 * starve due to the initialization thread execution.
 */
static void
slm_comp_init_loop(void *d)
{
	unsigned long init_schedule_current = 0;

	printc("Scheduler %ld: Running initialization thread.\n", cos_compid());
	/* If there are more components to initialize */
	while (init_schedule_current != ps_load(&init_schedule_off)) {
		/* Which is the next component to initialize? */
		compid_t client = init_schedule[init_schedule_current];
		struct schedinit_status *n;
		struct slm_thd *t;
		sched_param_t param[2];

		param[0] = sched_param_pack(SCHEDP_INIT, 0);
		param[1] = 0;

		extern struct slm_thd *thd_alloc_in(compid_t id, thdclosure_index_t idx, sched_param_t *parameters, int reschedule);

		/* Create the thread for initialization of the next component */
		t = thd_alloc_in(client, 0, param, 0);
		assert(t);

		n = &initialization_state[client];
		init_schedule_current++;

		/*
		 * This waits till init_done effective runs before
		 * moving on. We need to be highest-priority, so that
		 * we can direct switch to the initialization thread
		 * here.
		 *
		 * FIXME: if the initialization thread blocks, this
		 * will test the awkward code paths around waking on a
		 * block *before* the actual impulse you're blocking
		 * on is true. Always recheck your block conditions,
		 * kids!
		 */
		printc("\tScheduler %ld: initializing component %ld with thread %ld.\n", cos_compid(), client, t->tid);
		while (ps_load(&n->status) != SCHEDINIT_MAIN) {
			sched_tok_t             tok;

			assert(ps_load(&n->status) != SCHEDINIT_FREE);
			tok  = cos_sched_sync();
			slm_switch_to(slm_current_extern(), t, tok, 0);
		}
	}

	printc("Scheduler %ld, initialization completed.\n", cos_compid());
	slm_thd_deinit(slm_current_extern());		/* I'm out! */
	BUG();
}

/**
 * If the scheduler is supposed to initialize other components, this
 * should be called directly before the scheduler loop. Will create
 * threads to actuate the initialization process, and then the actual
 * threads to execute in the initializing components.
 */
int
slm_start_component_init(void)
{
	struct slm_thd_container *t;
	sched_param_t param[2];

	calculate_initialization_schedule();

	/* Note that the initialization protocol thread runs at the highest priority */
	param[0] = sched_param_pack(SCHEDP_INIT_PROTO, 0);
	param[1] = 0;

	typedef void (*thd_fn_t)(void *);
	struct slm_thd_container *thd_alloc(thd_fn_t fn, void *data, sched_param_t *parameters, int reschedule);

	t = thd_alloc(slm_comp_init_loop, NULL, param, 0);
	assert(t);

	return 0;
}
