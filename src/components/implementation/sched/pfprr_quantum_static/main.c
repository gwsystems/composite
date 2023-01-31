#include <cos_component.h>
#include <llprint.h>
#include <capmgr.h>

#include <static_slab.h>
#include <ps_list.h>
#include <ps.h>
#include <crt.h>

/***
 * A version of scheduling using a simple periodic timeout,
 * preemptive, fixed priority, round-robin scheduling, and uses the
 * capability manager to allocate threads, with local thread memory
 * tracked in static (allocate-only, finite) memory.
 */

#include <slm.h>
#include <quantum.h>
#include <fprr.h>
#include <slm_blkpt.c>

struct slm_resources_thd {
	thdcap_t cap;
	thdid_t  tid;
	compid_t comp;
};

struct slm_thd *slm_thd_static_cm_lookup(thdid_t id);

SLM_MODULES_COMPOSE_DATA();
SLM_MODULES_COMPOSE_FNS(quantum, fprr, static_cm);

struct crt_comp self;

SS_STATIC_SLAB(thd, struct slm_thd_container, MAX_NUM_THREADS);

/* Implementation for use by the other parts of the slm */
struct slm_thd *
slm_thd_static_cm_lookup(thdid_t id)
{
	return &ss_thd_get(id)->thd;
}

static inline struct slm_thd *
slm_thd_current(void)
{
	return &ss_thd_get(cos_thdid())->thd;
}

struct slm_thd *
slm_thd_current_extern(void)
{
	return slm_thd_current();
}

static struct slm_thd_container *
slm_thd_mem_alloc(thdcap_t _cap, thdid_t _tid, thdcap_t *thd, thdid_t *tid)
{
	struct slm_thd_container *t   = NULL;
	struct slm_thd_container *ret = NULL;

	ret = t = ss_thd_alloc_at_id(_tid);
	if (!t) assert(0);

	assert(_cap != 0 && _tid != 0);
	t->resources = (struct slm_resources_thd) {
		.cap  = _cap,
		.tid  = _tid,
		.comp = cos_compid()
	};

	*thd = _cap;
	*tid = _tid;

	return ret;
}

struct slm_thd_container *
slm_thd_alloc(thd_fn_t fn, void *data, thdcap_t *thd, thdid_t *tid)
{
	thdid_t _tid;
	thdcap_t _cap;
	struct slm_thd_container *ret = NULL;

	_cap = capmgr_thd_create(fn, data, &_tid);
	if (_cap <= 0) return NULL;

	return slm_thd_mem_alloc(_cap, _tid, thd, tid);
}

struct slm_thd_container *
slm_thd_alloc_in(compid_t cid, thdclosure_index_t idx, thdcap_t *thd, thdid_t *tid)
{
	struct slm_thd_container *ret = NULL;
	thdid_t _tid;
	thdcap_t _cap;

	_cap = capmgr_thd_create_ext(cid, idx, &_tid);
	if (_cap <= 0) return NULL;

	return slm_thd_mem_alloc(_cap, _tid, thd, tid);
}

void slm_mem_activate(struct slm_thd_container *t) { ss_thd_activate(t); }
void slm_mem_free(struct slm_thd_container *t) { return; }

struct slm_thd *
thd_alloc(thd_fn_t fn, void *data, sched_param_t *parameters, int reschedule)
{
	struct slm_thd_container *t;
	struct slm_thd *ret     = NULL;
	struct slm_thd *current = slm_thd_current();
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

	t = slm_thd_alloc(fn, data, &thdcap, &tid);
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
		if (slm_cs_exit_reschedule(current, SLM_CS_NONE)) ERR_THROW(NULL, free);
	} else {
		slm_cs_exit(NULL, SLM_CS_NONE);
	}

	ret = &t->thd;
done:
	return ret;
free:
	slm_mem_free(t);
	ret = NULL;
	goto done;
}

struct slm_thd *
thd_alloc_in(compid_t id, thdclosure_index_t idx, sched_param_t *parameters, int reschedule)
{
	struct slm_thd_container *t;
	struct slm_thd *ret     = NULL;
	struct slm_thd *current = slm_thd_current();
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

	t = slm_thd_alloc_in(id, idx, &thdcap, &tid);
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

	ret = &t->thd;
done:
	return ret;
free:
	slm_mem_free(t);
	ret = NULL;
	goto done;
}

thdid_t
sched_thd_create_closure(thdclosure_index_t idx)
{
	sched_param_t p = 0;
	struct slm_thd *t = thd_alloc_in(cos_inv_token(), idx, &p, 0);

	if (!t) return 0;

	return t->tid;
}

int
sched_thd_param_set(thdid_t tid, sched_param_t p)
{
	struct slm_thd *t = slm_thd_lookup(tid);
	sched_param_type_t type;
	unsigned int value;

	sched_param_get(p, &type, &value);

	if (!t) return -1;

	return slm_sched_thd_update(t, type, value);
}

int
sched_thd_delete(thdid_t tid)
{
	return 0;
}

int
sched_thd_exit(void)
{
	struct slm_thd *current = slm_thd_current();
	int i;

	slm_cs_enter(current, SLM_CS_NONE);
	slm_thd_deinit(current);
	for (i = 0; slm_cs_exit_reschedule(current, SLM_CS_NONE) && i < 16; i++) ;

	/* If we got here, something went wrong */
	BUG();

	return 0;
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
sched_thd_block(thdid_t dep_id)
{
	if (dep_id) return -1;

	return thd_block();
}

int
thd_wakeup(struct slm_thd *t)
{
	int ret;
	struct slm_thd *current = slm_thd_current();

	slm_cs_enter(current, SLM_CS_NONE);
	ret = slm_thd_wakeup(t, 0);
	if (ret < 0) {
		slm_cs_exit(NULL, SLM_CS_NONE);
		return ret;
	}

	return slm_cs_exit_reschedule(current, SLM_CS_NONE);
}

int
sched_thd_wakeup(thdid_t tid)
{
	struct slm_thd *t = slm_thd_lookup(tid);

	if (!t) return -1;

	return thd_wakeup(t);
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

cycles_t
sched_thd_block_timeout(thdid_t dep_id, cycles_t abs_timeout)
{
	cycles_t elapsed;

	if (dep_id) return 0;

	if (thd_block_until(abs_timeout)) {
		return 0;
	}

	return ps_tsc();
}

sched_blkpt_id_t
sched_blkpt_alloc(void)
{
	struct slm_thd *current = slm_thd_current();

	return slm_blkpt_alloc(current);
}

int
sched_blkpt_free(sched_blkpt_id_t id)
{
	return slm_blkpt_free(id);
}

int
sched_blkpt_trigger(sched_blkpt_id_t blkpt, sched_blkpt_epoch_t epoch, int single)
{
	struct slm_thd *current = slm_thd_current();

	return slm_blkpt_trigger(blkpt, current, epoch, single);
}

int
sched_blkpt_block(sched_blkpt_id_t blkpt, sched_blkpt_epoch_t epoch, thdid_t dependency)
{
	struct slm_thd *current = slm_thd_current();

	return slm_blkpt_block(blkpt, current, epoch, dependency);
}

int
thd_sleep(cycles_t c)
{
	cycles_t timeout = c + slm_now();

	return thd_block_until(timeout);
}

thdid_t
sched_aep_create_closure(thdclosure_index_t id, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, arcvcap_t *rcv)
{
	return 0;
}

int
main(void)
{
	extern int slm_start_component_init(void);

	slm_start_component_init();
	slm_sched_loop();
}

void
cos_init(void)
{
	struct slm_thd_container *t;
	struct cos_compinfo *boot_info = cos_compinfo_get(cos_defcompinfo_curr_get());
	thdcap_t thdcap;
	thdid_t tid;

	cos_meminfo_init(&(boot_info->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
	cos_defcompinfo_sched_init();

	t = slm_thd_alloc(slm_idle, NULL, &thdcap, &tid);
	if (!t) BUG();

	slm_init(thdcap, tid);
}
