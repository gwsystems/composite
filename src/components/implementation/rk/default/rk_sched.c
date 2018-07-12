#include <res_spec.h>
#include <vk_types.h>
#include "rk_sched.h"
#include <llprint.h>
#include <hypercall.h>
#include <schedinit.h>

/*
 * TODO: Doesn't look like we need a recursive lock!
 * Wonder why we had "cos_nesting" before!
 * Confirm that and perhaps remove all the commented code related
 * to recursive locking!
 */
//#define CS_RECURSE_LIMIT (1<<5)
//volatile unsigned int cs_recursive = 0;

/* NOTE: This needs to be updated as we support more apps!! */
static char *app_names[RK_APPS_MAX] = {
		"udpserv",
		"http",
		"iperf",
	};

static char *stub_names[RK_STUBS_MAX] = {
		"udpstub",
		"httpstub",
		"iperfstub",
		"kitcistub",
		"kitschstub",
		"kittostub",
		"tftpstub",
		"hsstub",
	};

static spdid_t app_spdid[RK_APPS_MAX] = { 0 };
static spdid_t stub_spdid[RK_STUBS_MAX] = { 0 };
static struct sl_thd *stub_thd[RK_STUBS_MAX] = { NULL };

spdid_t rk_child_app[RK_APPS_MAX] = { 0 };

extern cbuf_t parent_schedinit_child();

static struct sl_thd *stubreqs[RK_STUBREQS_MAX] = { NULL };

int
rk_child_fakereq_set(struct sl_thd *t, char *reqname)
{
	int i, instance = -1;
	assert(t && reqname);
	assert(strcmp(reqname, "stub_") == 0);

	for (i = 0; i < RK_STUBREQS_MAX; i++) {
		int ret = 0;

		if (stubreqs[i] != NULL) continue;

		ret = ps_cas((unsigned long *)&stubreqs[i],(unsigned long)NULL, (unsigned long)t);
		if (ret == 0) continue;

		instance = i;
		reqname[5] = instance + 48;
		reqname[6] = '\0';
	}

	/* -1 if there is no free slot! */
	return instance;
}

static void
rk_child_fakereq_reset(int instance)
{
	int ret = 0;
	struct sl_thd *t = stubreqs[instance];

	assert(t);

	ret = ps_cas((unsigned long *)&stubreqs[instance], (unsigned long)t, (unsigned long)NULL);
	assert(ret == 1);
}

static struct sl_thd *
rk_child_fakereq_get(int instance)
{
	struct sl_thd *t = NULL;

	assert(instance >= 0 && instance < RK_STUBREQS_MAX);
	t = stubreqs[instance];
	assert(t);

	rk_child_fakereq_reset(instance);

	return t;
}

void
rk_curr_thd_set_prio(int prio)
{
	struct sl_thd *t = sl_thd_curr();
	union sched_param_union spprio = {.c = {.type = SCHEDP_PRIO, .value = prio}};

	sl_thd_param_set(t, spprio.v);
}

static int
rk_rump_thd_param_set(struct sl_thd *t)
{
	union sched_param_union spprio = {.c = {.type = SCHEDP_PRIO, .value = RK_RUMP_THD_PRIO}};

	sl_thd_param_set(t, spprio.v);

	return 0;
}

#define INTR_BUDGET_US 1000
#define INTR_PERIOD_US 1000

static int
rk_intr_thd_param_set(struct sl_thd *t, int own_tcap)
{
	union sched_param_union spprio = {.c = {.type = SCHEDP_PRIO, .value = RK_INTR_THD_PRIO}};

	if (own_tcap) {
		sl_thd_param_set(t, sched_param_pack(SCHEDP_WINDOW, INTR_PERIOD_US));
		sl_thd_param_set(t, sched_param_pack(SCHEDP_BUDGET, INTR_BUDGET_US));
	}
	sl_thd_param_set(t, spprio.v);

	return 0;
}

static int
rk_subsys_thd_param_set(struct sl_thd *t)
{
	union sched_param_union spprio = {.c = {.type = SCHEDP_PRIO, .value = TIMER_PRIO}};
	union sched_param_union spexec = {.c = {.type = SCHEDP_BUDGET, .value = TM_SUB_BUDGET_US}};
	union sched_param_union spperiod = {.c = {.type = SCHEDP_WINDOW, .value = TM_SUB_PERIOD_US}};

	sl_thd_param_set(t, spprio.v);
	sl_thd_param_set(t, spexec.v);
	sl_thd_param_set(t, spperiod.v);

	return 0;
}

struct sl_thd *
rk_rump_thd_init(struct cos_aep_info *aep)
{
	struct sl_thd *t = NULL;

	t = sl_thd_init_ext(aep, NULL);
	assert(t);

	rk_rump_thd_param_set(t);

	return t;
}

static struct sl_thd *
rk_subsys_thd_init(thdcap_t thd, arcvcap_t rcv, tcap_t tc, asndcap_t snd, int is_sched)
{
	static int only_once = 0;
	struct cos_defcompinfo sub_defci;
	struct cos_compinfo *subci = cos_compinfo_get(&sub_defci);
	struct cos_aep_info *subaep = cos_sched_aep_get(&sub_defci);
	struct sl_thd *t = NULL;

	assert(is_sched);

	assert(only_once == 0);
	only_once ++;

	subci->captbl_cap = BOOT_CAPTBL_SELF_CT;
	subaep->thd = thd;
	subaep->rcv = rcv;
	subaep->tc = tc;

	t = sl_thd_comp_init(&sub_defci, is_sched);
	assert(t);
	t->sndcap = snd;

	rk_subsys_thd_param_set(t);

	return t;
}

struct sl_thd *
rk_rump_thd_alloc(cos_thd_fn_t fn, void *data)
{
	struct sl_thd *t = NULL;

	t = sl_thd_alloc(fn, data);
	assert(t);

	rk_rump_thd_param_set(t);

	return t;
}

struct sl_thd *
rk_intr_aep_alloc(cos_aepthd_fn_t fn, void *data, int own_tcap, cos_channelkey_t key)
{
	struct sl_thd *t = NULL;

	t = sl_thd_aep_alloc(fn, data, own_tcap, key, 0, 0);
	assert(t);

	rk_intr_thd_param_set(t, own_tcap);

	return t;
}

struct sl_thd *
rk_intr_aep_init(struct cos_aep_info *aep, int own_tcap)
{
	struct sl_thd *t = NULL;

	t = sl_thd_init_ext(aep, NULL);
	assert(t);

	rk_intr_thd_param_set(t, own_tcap);

	return t;
}

void
rk_rump_thd_yield_to(struct bmk_thread *c, struct bmk_thread *n)
{
	struct sl_thd *curr = sl_thd_curr();
	thdid_t ntid = get_cos_thdid(n), ctid = get_cos_thdid(c);
	struct sl_thd *t = sl_thd_lkup(ntid);

	assert(ctid == cos_thdid());
	assert(ntid > 0 && t);

	sl_thd_yield(ntid);
}

void
rk_sched_stub(struct sl_thd *t)
{
	int i = 0;

	for (i = 0; i < RK_STUBS_MAX; i++) {
		if (stub_thd[i] == t) break;
	}
	assert(i < RK_STUBS_MAX);

	/* TODO: prio only matters because they're non-scheduling threads.. */
	sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, RK_STUBINIT_PRIO));
}

void
rk_sched_loop(void)
{
	printc("Notifying parent scheduler...\n");
	parent_schedinit_child();

#ifdef CFE_RK_MULTI_CORE
	sl_sched_loop_nonblock();
#else
	sl_sched_loop();
#endif
}

void
rk_sched_init(microsec_t period)
{
	sl_init(period);
}

void
rk_rump_thd_wakeup(struct bmk_thread *w)
{
	sl_thd_wakeup(get_cos_thdid(w));
}

int
rk_rump_thd_block_timeout(struct bmk_thread *c, unsigned long long timeout_nsec)
{
	cycles_t abs_timeout = sl_usec2cyc(timeout_nsec / 1000);

	assert(get_cos_thdid(c) == cos_thdid());

	if (sl_thd_block_timeout(0, abs_timeout)) return 1;

	return 0;
}

void
rk_rump_thd_block(struct bmk_thread *c)
{
	assert(get_cos_thdid(c) == cos_thdid());

	sl_thd_block(0);
}

void
rk_rump_thd_yield(void)
{
	sl_thd_yield(0);
}

void
rk_rump_thd_exit(void)
{
	sl_thd_exit();
}

static void
rk_sched_cs_enter(void)
{
//	if (sl_cs_owner()) {
//		assert(cs_recursive);
//		goto recurse;
//	}

	sl_cs_enter();

//recurse:
//	__sync_add_and_fetch(&cs_recursive, 1);
//	assert(cs_recursive < CS_RECURSE_LIMIT); /* make sure it's not taken too many times */
}

static void
rk_sched_cs_exit(void)
{
	assert(sl_cs_owner());

//	assert(cs_recursive);
//
//	__sync_sub_and_fetch(&cs_recursive, 1);
//	if (!cs_recursive)
		sl_cs_exit();
}

void
rk_intr_disable(void)
{
	rk_sched_cs_enter();
}

void
rk_intr_enable(void)
{
	rk_sched_cs_exit();
}

static void
rk_app_spdids(void)
{
	int i;

	for (i = 0; i < RK_APPS_MAX; i++) {
		PRINTC("APP: %s len: %d\n", app_names[i], strlen(app_names[i]));
		spdid_t app = hypercall_comp_id_get(app_names[i]);

		if (!app) continue;
		app_spdid[i] = app;
		PRINTC("RK APP in the system: %s spdid: %d\n", app_names[i], app_spdid[i]);
	}
}

static void
rk_stub_spdids(void)
{
	int i;

	for (i = 0; i < RK_STUBS_MAX; i++) {
		PRINTC("STUB: %s len: %d\n", stub_names[i], strlen(stub_names[i]));
		spdid_t stub = hypercall_comp_id_get(stub_names[i]);

		if (!stub) continue;
		stub_spdid[i] = stub;
		PRINTC("RK STUB in the system: %s spdid: %d\n", stub_names[i], stub_spdid[i]);
	}
}

static char *
rk_stub_find(spdid_t s)
{
	int i = 0;

	assert(s);
	for (i = 0; i < RK_STUBS_MAX; i++) {
		if (stub_spdid[i] == s) break;
	}

	if (i >= RK_STUBS_MAX) return NULL;

	return stub_names[i];
}

static int
rk_stub_setthd(spdid_t s, struct sl_thd *t)
{
	int i = 0;

	assert(s && t);
	for (i = 0; i < RK_STUBS_MAX; i++) {
		if (stub_spdid[i] != s) continue;

		assert(stub_thd[i] == NULL);
		stub_thd[i] = t;

		return 0;
	}

	return -1;
}

spdid_t
rk_stub_findspd(char *name)
{
	int i = 0;

	assert(name);
	for (i = 0; i < RK_STUBS_MAX; i++) {
		if (strcmp(stub_names[i], name) == 0) break;
	}

	if (i >= RK_STUBS_MAX) return 0;

	return stub_spdid[i];
}

spdid_t
rk_stub_iter(void)
{
	static int i = 0;
	int j;

	if (i >= RK_STUBS_MAX || !stub_spdid[i]) return 0;

	j = i;
	i++;
	return stub_spdid[j];
}

static char *
rk_app_find(spdid_t s)
{
	int i = 0;

	assert(s);
	for (i = 0; i < RK_APPS_MAX; i++) {
		if (app_spdid[i] == s) break;
	}

	if (i >= RK_APPS_MAX) return NULL;

	return app_names[i];
}

spdid_t
rk_app_findspd(char *name)
{
	int i = 0;

	assert(name);
	for (i = 0; i < RK_APPS_MAX; i++) {
		if (strcmp(app_names[i], name) == 0) break;
	}

	if (i >= RK_APPS_MAX) return 0;

	return app_spdid[i];
}

void
rk_child_initthd_walk(char *cmdline)
{
	int remaining = 0;
	spdid_t child;
	comp_flag_t childflags;
	int num_child = 0;
	int direct_app = 0;

	rk_app_spdids();
	rk_stub_spdids();
	assert(RK_STUBREQS_MAX < 10); /* single digit for avoiding sprintf */

	while ((remaining = hypercall_comp_child_next(cos_spd_id(), &child, &childflags)) >= 0) {
		struct sl_thd *t = NULL;
		char *appname = NULL;

		assert(child);
		/* no child schedulers on top of RK!! */
		assert(!(childflags & COMP_FLAG_SCHED));
		if ((appname = rk_app_find(child)) != NULL) {
			//assert(rk_child_app == 0); /* only a single direct child app? */
			assert(direct_app < RK_APPS_MAX);
			rk_child_app[direct_app] = child;
			PRINTC("Direct child RK APP: %s spdid: %u\n", appname, rk_child_app[direct_app]);
			direct_app++;

			goto done; /* apps taken care from rumpkernel layer */
		}

		PRINTC("STUB CHILD: %u\n", child);
		appname = rk_stub_find(child);
		assert(appname);
		num_child ++;
done:
		strcat(cmdline, appname);
		if (!remaining) break;
		strncat(cmdline, ".", 1);
	}
	if (cmdline[strlen(cmdline)-1] == '.') cmdline[strlen(cmdline)-1] = '\0';

	PRINTC("Number of \"stub\" child components: %d, cmdline: %s\n", num_child, cmdline);
}

/* creates initthds for non rumpkernel apps.. for sinv/async communication stub components */
struct sl_thd *
rk_child_stubcomp_init(char *name)
{
	struct cos_defcompinfo child_dci;
	struct sl_thd *t = NULL;
	int ret;
	spdid_t child = rk_stub_findspd(name);

	if (!child) return t;

	cos_defcompinfo_childid_init(&child_dci, child);

	t = sl_thd_initaep_alloc(&child_dci, 0, 0, 0, 0, 0, 0);
	assert(t);
	ret = rk_stub_setthd(child, t);
	assert(ret == 0);
	PRINTC("Initialized child \"stub\" %s component: %u, initthd: %u\n", name, child, sl_thd_thdid(t));

	return t;
}

struct sl_thd *
rk_child_fakethd_init(char *name)
{
	char tmpname[RK_NAME_MAX] = { '\0' }, *t = tmpname;
	char *tok = NULL;
	int instance = 0;

	strcpy(tmpname, name);
	tok = strtok_r(t, "_", &t);

	if (strcmp(tok, "stub") != 0) return NULL;
	tok = strtok_r(NULL, "_", &t);
	if (strlen(tok) != 1) return NULL;
	instance = tok[0] - 48;
	if (instance < 0 || instance >= RK_STUBREQS_MAX) return NULL;

	return rk_child_fakereq_get(instance);
}
