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

#include <sl.h>
#include <res_spec.h>
#include <barrier.h>
#include <init.h>
#include <sched_info.h>

u32_t cycs_per_usec = 0;

#define INITIALIZE_PRIO 1
#define INITIALIZE_PERIOD_MS (4000)
#define INITIALIZE_BUDGET_MS (2000)

#define FIXED_PRIO 2
#define FIXED_PERIOD_MS (10000)
#define FIXED_BUDGET_MS (4000)

typedef enum
{
	SCHEDINIT_FREE,
	SCHEDINIT_INITING,
	SCHEDINIT_PARINIT,
	SCHEDINIT_MAIN, /* main, or parallel_main depending on main_type */
} schedinit_t;

struct schedinit_status {
	struct simple_barrier barrier;
	schedinit_t           status;
	init_main_t           main_type;
};

static struct schedinit_status initialization_state[MAX_NUM_COMPS] = {0};

/* This schedule is used by the initializer_thd to ascertain order */
static unsigned long init_schedule_off            = 0;
static compid_t      init_schedule[MAX_NUM_COMPS] = {0};
/* internalizer threads simply orchestrate the initialization */
static struct sl_thd *__initializer_thd[NUM_CPU] CACHE_ALIGNED;

void
schedinit_next(compid_t cid)
{
	struct schedinit_status *s;

	printc("\tSched %ld: %ld is the %ldth component to initialize\n", cos_compid(), cid, init_schedule_off);
	init_schedule[init_schedule_off] = cid;
	init_schedule_off++;
	s = &initialization_state[cid];
	assert(s->status == SCHEDINIT_FREE);
	*s = (struct schedinit_status){
	  .status    = SCHEDINIT_INITING,
	  .main_type = INIT_MAIN_NONE,
	};
	simple_barrier(&s->barrier);

	return;
}

void
init_done(int parallel_init, init_main_t cont)
{
	compid_t                 client = (compid_t)cos_inv_token();
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
		sl_thd_exit(); /* No main? No more execution! */
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
	sl_thd_exit();
	BUG();
	while (1)
		;
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
initialization_thread(void *d)
{
	unsigned long init_schedule_current = 0;

	printc("Scheduler %ld: Running initialization thread.\n", cos_compid());
	/* If there are more components to initialize */
	while (init_schedule_current != ps_load(&init_schedule_off)) {
		/* Which is the next component to initialize? */
		compid_t                 client = init_schedule[init_schedule_current];
		struct schedinit_status *n;
		struct sl_thd *          t;

		/* Create the thread for initialization of the next component */
		t = sched_childinfo_init_component(client);
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
		 * kids! Alternative, we can block using something
		 * like sl_thd_block_periodic(0)
		 */
		while (ps_load(&n->status) != SCHEDINIT_MAIN) {
			assert(ps_load(&n->status) != SCHEDINIT_FREE);
			sl_thd_yield(sl_thd_thdid(t));
		}
	}

	printc("Scheduler %ld, initialization completed.\n", cos_compid());
	sl_thd_exit(); /* I'm out! */
	BUG();
}

void
sched_child_init(struct sched_childinfo *schedci)
{
	struct sl_thd *initthd = NULL;

	assert(schedci);
	initthd = sched_child_initthd_get(schedci);
	assert(initthd);
	sl_thd_param_set(initthd, sched_param_pack(SCHEDP_PRIO, FIXED_PRIO));
	sl_thd_param_set(initthd, sched_param_pack(SCHEDP_WINDOW, FIXED_PERIOD_MS));
	sl_thd_param_set(initthd, sched_param_pack(SCHEDP_BUDGET, FIXED_BUDGET_MS));
}

static u32_t cpubmp[NUM_CPU_BMP_WORDS] = {0};

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);

	printc("Scheduler %ld initializing.\n", cos_compid());
	cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
	cos_init_args_cpubmp(cpubmp);
}

void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	int i;

	if (!init_core) cos_defcompinfo_sched_init();

	sl_init_cpubmp(SL_MIN_PERIOD_US, cpubmp);
	/* parse through the components we're supposed to boot... */
	sched_childinfo_init();
	/* Then run the thread that boots the components */
	__initializer_thd[cos_cpuid()] = sl_thd_alloc(initialization_thread, NULL);
	assert(__initializer_thd[cos_cpuid()]);
	sl_thd_param_set(__initializer_thd[cos_cpuid()], sched_param_pack(SCHEDP_PRIO, INITIALIZE_PRIO));
	sl_thd_param_set(__initializer_thd[cos_cpuid()], sched_param_pack(SCHEDP_WINDOW, INITIALIZE_BUDGET_MS));
	sl_thd_param_set(__initializer_thd[cos_cpuid()], sched_param_pack(SCHEDP_BUDGET, INITIALIZE_PERIOD_MS));

	return;
}

void
parallel_main(coreid_t cid)
{
	sl_sched_loop_nonblock();
	PRINTLOG(PRINT_ERROR, "Should never have reached this point!!!\n");
	assert(0);
}
