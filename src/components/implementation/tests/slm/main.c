#include <cos_component.h>
#include <llprint.h>
#include <static_slab.h>
#include <ps_list.h>
#include <crt.h>

/***
 * A version of scheduling using a simple periodic timeout, and fixed
 * priority, round-robin scheduling.
 */

#include <slm.h>
#include <quantum.h>
#include <fprr.h>

/* Quick hack at the thread memory operations */

struct crt_comp self;

struct slm_resources_thd {
	struct crt_thd crt_res;
};

SLM_MODULES_COMPOSE_DATA();

SS_STATIC_SLAB(thd, struct slm_thd_container, MAX_NUM_THREADS);

/* Implementation for use by the other parts of the slm */
struct slm_thd *
slm_thd_preallocslab_lookup(thdid_t id)
{
	return &ss_thd_get(id)->thd;
}

static inline struct slm_thd *
slm_thd_current(void)
{
	return slm_thd_lookup(cos_thdid());
}

SLM_MODULES_COMPOSE_FNS(quantum, fprr, preallocslab);

typedef void (*thd_fn_t)(void *);

struct slm_thd_container *
slm_mem_crtstatic_alloc(thd_fn_t fn, void *data, thdcap_t *thd, thdid_t *tid)
{
	struct slm_thd_container *t   = NULL;
	struct slm_thd_container *ret = NULL;
	struct crt_thd crt_thd        = { 0 };
	int i;

	if (crt_thd_create(&crt_thd, &self, fn, data) < 0) ERR_THROW(NULL, done);
	ret = t = ss_thd_alloc_at_id(crt_thd.tid);
	if (!t) assert(0);

	assert(crt_thd.cap != 0 && crt_thd.tid);
	t->resources.crt_res = crt_thd; /* copy the resources! */

	*thd = crt_thd.cap;
	*tid = crt_thd.tid;
done:
	return ret;
}

static inline struct slm_thd *
slm_current(void)
{
	return slm_thd_lookup(cos_thdid());
}

void
slm_mem_crtstatic_activate(struct slm_thd_container *t)
{
	ss_thd_activate(t);
}

struct slm_thd_container *
slm_mem_alloc(thd_fn_t fn, void *data, thdcap_t *cap, thdid_t *tid)
{
	return slm_mem_crtstatic_alloc(fn, data, cap, tid);
}

void slm_mem_activate(struct slm_thd_container *t) { slm_mem_crtstatic_activate(t); }
void slm_mem_free(struct slm_thd_container *t) { return; }

struct slm_thd_container *
thd_alloc(thd_fn_t fn, void *data, sched_param_t *parameters, int reschedule)
{
	struct slm_thd_container *t;
	struct slm_thd_container *ret = NULL;
	struct slm_thd *current       = slm_thd_current();
	thdcap_t thdcap;
	thdid_t tid;
	int i;

	/*
	 * If this condition is true, we are likely in the
	 * initialization sequence in the idle or scheduler threads...
	 */
	if (!current) {
		current = slm_thd_special();
		assert(current);
	}

	ret = t = slm_mem_alloc(fn, data, &thdcap, &tid);
	if (!t) ERR_THROW(NULL, done);

	slm_cs_enter(current, SLM_CS_NONE);
	if (slm_thd_init(&t->thd, thdcap, tid)) ERR_THROW(NULL, free);

	for (i = 0; parameters[i] != 0; i++) {
		sched_param_type_t type;
		unsigned int value;

		sched_param_get(parameters[i], &type, &value);
		if (slm_sched_thd_update(&t->thd, type, value)) ERR_THROW(NULL, free);
	}
	slm_mem_activate(t);

	if (reschedule) {
		if (slm_cs_exit_reschedule(current, SLM_CS_NONE)) ERR_THROW(NULL, done);
	} else {
		slm_cs_exit(NULL, SLM_CS_NONE);
	}
done:
	return ret;
free:
	slm_mem_free(t);
	ret = NULL;
	goto done;
}

int
thd_block(void)
{
	struct slm_thd *current = slm_thd_current();
	int ret;

	slm_cs_enter(current, SLM_CS_NONE);
        ret = slm_thd_block(current);
	if (!ret) ret = slm_cs_exit_reschedule(current, SLM_CS_NONE);
	else      slm_cs_exit(NULL, SLM_CS_NONE);

	return ret;
}

int
thd_wakeup(struct slm_thd_container *t)
{
	int ret;
	struct slm_thd *current = slm_thd_current();

	slm_cs_enter(current, SLM_CS_NONE);
	ret = slm_thd_wakeup(&t->thd, 0);
	if (ret < 0) {
		slm_cs_exit(NULL, SLM_CS_NONE);
		return ret;
	}

	return slm_cs_exit_reschedule(current, SLM_CS_NONE);
}

static int
thd_block_until(cycles_t timeout)
{
	struct slm_thd *current = slm_thd_current();

	slm_cs_enter(current, SLM_CS_NONE);
	if (slm_timer_add(current, timeout)) goto done;
	if (slm_thd_block(current)) {
		slm_timer_cancel(current);
	}
done:
	return slm_cs_exit_reschedule(current, SLM_CS_NONE);
}

int
thd_sleep(cycles_t c)
{
	cycles_t timeout = c + slm_now();

	return thd_block_until(timeout);
}

unsigned long num_success = 0;
struct slm_thd_container *done_thd;

#define TIMER_TEST_LEN     5000000	        /* run the test for 5 seconds */
#define TIMER_ERROR_BOUND  (10000+10000)	/* number of microseconds in a quantum + a fudge factor*/
#define NTIMERS            4
unsigned long timers[NTIMERS + 1] = { 60000, 90000, 130000, 170000, 0 };
unsigned long success_cnt_down = NTIMERS;

void
timer_fn(void *data)
{
	int i;
	cycles_t period = slm_usec2cyc((unsigned long)data);
	cycles_t next, prev, curr, start, end;

	start = curr = prev = slm_now();
	end = start + slm_usec2cyc(TIMER_TEST_LEN);

	for (i = 0; cycles_greater_than(end, curr); i++) {
		next = curr + period;
		if (thd_block_until(next)) BUG();
		curr = slm_now();

		if (!cycles_greater_than(curr, prev)) {
			printc("FAILURE: cycles progress backwards (from %lld to %lld)\n", prev, curr);
			BUG();
		}
		if (cycles_greater_than(next, curr)) {
			printc("FAILURE: woke up @ %lld before timeout @ %lld\n", curr, next);
			BUG();
		}
		if (curr - prev > period + (slm_usec2cyc(TIMER_ERROR_BOUND))) {
			printc("FAILURE: woke up more than a timer tick + 50 microseconds later than anticipated (%lld vs. %lld).\n",
			       curr, prev + period + slm_usec2cyc(TIMER_ERROR_BOUND));
			BUG();
		}
		prev = curr;
	}

	if (ps_faa(&success_cnt_down, -1) == 1) {
		printc("SUCCESS: timers within the expected tolerances.\n");
		ps_faa(&num_success, 1);
		thd_wakeup(done_thd);
	}
	thd_block();
}

/*
 * Test the accuracy of the timer logic. Multiple threads wait for
 * timeouts of varying lengths, and validate that they woke after
 * there were supposed to, and not too late.
 */
void
timer_test(unsigned int prio_base)
{
	int i;
	sched_param_t param[2];

	param[0] = sched_param_pack(SCHEDP_PRIO, prio_base + 1);
	param[1] = 0;

	for (i = 0; timers[i] != 0; i++) {
		if (!thd_alloc(timer_fn, (void *)timers[i], param, 0)) assert(0);
	}
}

#define NPING_PONG 2
#define PING_PONG_ITER 100
struct pingpong_data {
	unsigned long block_first;
 	struct slm_thd_container *other_thd;
};
volatile struct pingpong_data ppdata[NPING_PONG];

void
pingpong_fn(void *data)
{
	struct pingpong_data *d = data;
	int i;

	while (!d->other_thd) ;

	if (d->block_first) {
		thd_block();
	}
	for (i = 0; i < PING_PONG_ITER; i++) {
		thd_wakeup(d->other_thd);
		thd_block();
	}

	printc("SUCCESS: Ping-pong complete.\n");
	ps_faa(&num_success, 1);

	thd_block();
	BUG();
}

/*
 * Lets set up a ring of block/wakeup similar to the lmbench ring
 * test. a wakes b wakes c wakes ... wakes a.
 */
void
pingpong_block_test(unsigned int prio_base)
{
	int i;
	sched_param_t param[2];

	param[0] = sched_param_pack(SCHEDP_PRIO, prio_base);
	param[1] = 0;

	for (i = 0; i < NPING_PONG; i++) {
		struct slm_thd_container *t;

		t = thd_alloc(pingpong_fn, (void *)&ppdata[i], param, 0);
		if (!t) BUG();
		ppdata[(i+1) % NPING_PONG] = (struct pingpong_data) {
			.other_thd = t,
			.block_first = (i != 0)
		};
	}
}

#define INTERLEAVE_ITER 16
#define NINTERLEAVE 2
unsigned long off = 0;
struct interleave_cnt {
	unsigned long long cnt;
	thdid_t id;
};
struct interleave_cnt cnts[INTERLEAVE_ITER] = { 0 };

void
interleave_fn(void *d)
{
	int i;
	thdid_t previd;
	unsigned long long tot, avgcnt;

	while (off < INTERLEAVE_ITER) {
		unsigned long localoff = ps_load(&off);
		struct interleave_cnt *c = &cnts[off];

		if (c->id == 0) {
			ps_store(&c->id, cos_thdid());
		}
		if (c->id != cos_thdid()) {
			ps_store(&off, ps_load(&off) + 1);
			continue;
		}
		while (localoff == ps_load(&off)) {
			int spin = 1 << 22;

			assert(c->id == cos_thdid());
			while (spin) {
				ps_store(&spin, ps_load(&spin) - 1);
			}
			c->cnt++;
		}
	}

	tot = 0;
	previd = cnts[0].id;
	for (i = 1; i < INTERLEAVE_ITER - 1; i++) {
		tot += cnts[i].cnt;
		assert(cnts[i].id != previd);
		previd = cnts[i].id;
	}
	avgcnt = tot / (INTERLEAVE_ITER - 2);
	for (i = 1; i < INTERLEAVE_ITER - 1; i++) {
		long long diff = avgcnt - cnts[i].cnt;
		unsigned long posdiff = diff < 0 ? -diff : diff;
		/* allow 10% variation across ticks */
		assert(posdiff <= (avgcnt / 10) + 1);
	}
	if (previd == cos_thdid()) {
		printc("SUCCESS: Interleaving correct.\n");
		ps_faa(&num_success, 1);
	}

	thd_block();
	BUG();
}

/*
 * Will timer ticks cause us to properly switch back and forth? When
 * we do, will we use roughly the same amount of CPU time?
 */
void
interleave_test(unsigned int prio_base)
{
	int i;
	sched_param_t param[2];

	param[0] = sched_param_pack(SCHEDP_PRIO, prio_base);
	param[1] = 0;

	for (i = 0; i < NINTERLEAVE; i++) {
		if (!thd_alloc(interleave_fn, NULL, param, 0)) BUG();
	}
}

void
done_fn(void *d)
{
	thd_block();
	printc("SUCCESS for %ld / 3. Tests complete.\n", num_success);
	thd_block();
}

int
main(void)
{
	sched_param_t param[2];

	param[0] = sched_param_pack(SCHEDP_PRIO, 10);
	param[1] = 0;

	pingpong_block_test(2);
	interleave_test(4);
	timer_test(6);
	done_thd = thd_alloc(done_fn, NULL, param, 0);
	if (!done_thd) BUG();
	slm_sched_loop();
}

void
cos_init(void)
{
	struct crt_thd t;
	struct cos_compinfo *boot_info = cos_compinfo_get(cos_defcompinfo_curr_get());

	cos_meminfo_init(&(boot_info->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
	cos_defcompinfo_sched_init();

	if (crt_booter_create(&self, "self", cos_compid(), 0)) assert(0);
	if (crt_thd_create(&t, &self, slm_idle, NULL)) assert(0);

	slm_init(t.cap, t.tid);
}
