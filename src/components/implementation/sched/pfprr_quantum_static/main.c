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
#include <slm_modules.h>

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

struct slm_thd *
slm_thd_from_container(struct slm_thd_container *c) {
	return &c->thd;
}

struct slm_thd_container *
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

void slm_thd_mem_activate(struct slm_thd_container *t) { ss_thd_activate(t); }
/* TODO */
void slm_thd_mem_free(struct slm_thd_container *t) { return; }

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
	for (i = 0; slm_cs_exit_reschedule(current, SLM_CS_NONE) && i < 16; i++) printc("$$\n");

	/* If we got here, something went wrong */
	BUG();

	return 0;
}

int
sched_thd_yield_to(thdid_t t)
{
	struct slm_thd *current = slm_thd_current();
	struct slm_thd *to = slm_thd_lookup(t);
	int ret;

	if (!to) return -1;

//printc("yield to: %d\n", t);
	assert(current != to);
	slm_cs_enter(current, SLM_CS_NONE);
        slm_sched_yield(current, to);
	//ret = slm_cs_exit_reschedule(current, SLM_CS_CHECK_TIMEOUT);
	ret = slm_cs_exit_reschedule(current, SLM_CS_NONE);

	//printc("yield ret\n");
	return ret;
}


void
sched_set_tls(void* tls_addr)
{
	struct slm_thd *current = slm_thd_current();
	thdcap_t thdcap = current->thd;

	capmgr_set_tls(thdcap, tls_addr);
}

arcvcap_t sched_arcv_create(thdid_t tid) { BUG(); return 0; }

asndcap_t sched_asnd_create(thdid_t tid) { BUG(); return 0; }

int
sched_asnd(thdid_t tid)
{
	struct slm_thd *thd = slm_thd_lookup(tid);

	assert(thd->asnd);
	int ret = cos_asnd(thd->asnd, 1);

	return ret;
}

int
sched_arcv(thdid_t tid)
{
	int rcvd = 0, pending = 0;
	struct slm_thd *thd = slm_thd_lookup(tid);

	assert(thd->rcv);
	pending = cos_rcv(thd->rcv, RCV_ALL_PENDING, &rcvd);
	assert(pending == 0);

	return rcvd;
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
	return -1;
}

unsigned long
sched_get_cpu_freq(void)
{
	return slm_get_cycs_per_usec();
}

thdcap_t idlecap;

void
parallel_main(coreid_t cid)
{
	if (cid == 0) printc("Starting scheduler loop...\n");
	slm_sched_loop_nonblock();
}

void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	struct slm_thd_container *t;
	struct cos_dcb_info *dcb;
	thdcap_t thdcap;
	thdid_t tid;

	//if (!init_core) cos_defcompinfo_sched_init();
	cos_defcompinfo_sched_init();

	t = slm_thd_alloc(slm_idle, NULL, &thdcap, &tid, &dcb);
	//printc("\tcreate done: %d\n", tid);
	if (!t) BUG();
	idlecap = thdcap;

	vaddr_t init_dcb = capmgr_sched_initdcb_get();

	slm_init(thdcap, tid, (struct cos_dcb_info *)init_dcb, dcb);
}

void
cos_init(void)
{
	struct cos_compinfo *boot_info = cos_compinfo_get(cos_defcompinfo_curr_get());

	cos_meminfo_init(&(boot_info->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	extern void calculate_initialization_schedule(void);
	calculate_initialization_schedule();
	cos_defcompinfo_init();

	if (capmgr_scb_mapping()) BUG();
}
