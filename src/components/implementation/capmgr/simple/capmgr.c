/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2020, The George Washington University
 * Author: Gabe Parmer, gparmer@gwu.edu
 */

#include <cos_debug.h>
#include <consts.h>
#include <static_alloc.h>
#include <crt.h>

#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <initargs.h>
#include <addr.h>

struct cm_rcv {
	struct crt_rcv rcv;
	struct cm_comp *sched;
};

struct cm_comp {
	struct crt_comp comp;
	struct cm_rcv *sched_rcv;    /* rcv cap for this scheduler or NULL if not a scheduler */
	struct cm_rcv *sched_parent; /* rcv cap for this scheduler's scheduler, or NULL */
};

struct cm_thd {
	struct sl_thd  *sched_thd; /* scheduler data-structure for the thread */
	struct crt_thd  thd;
	struct cm_comp *sched;
};

struct cm_asnd {
	struct crt_asnd asnd;
};

/*
 * Shared memory should be manager -> client, and between
 * point-to-point channel recipients
 */
#define MM_MAPPINGS_MAX 3

struct mm_mapping {
	compid_t cid;
	vaddr_t  addr;
};

typedef unsigned int cbuf_t;
struct mm_page {
	void *page;
	struct mm_mapping mappings[MM_MAPPINGS_MAX];
};

SA_STATIC_ALLOC(comp, struct cm_comp, MAX_NUM_COMPS);
SA_STATIC_ALLOC(thd, struct cm_thd, MAX_NUM_THREADS);
/* These size values are somewhat arbitrarily chosen */
SA_STATIC_ALLOC(rcv, struct cm_rcv, MAX_NUM_THREADS);
SA_STATIC_ALLOC(asnd, struct cm_asnd, MAX_NUM_THREADS);

/* 64 MiB */
#define MB2PAGES(mb) (round_up_to_page(mb * 1024 * 1024) / PAGE_SIZE)
#define MM_NPAGES (MB2PAGES(64))
SA_STATIC_ALLOC(page, struct mm_page, MM_NPAGES);

static struct cm_comp *
cm_self(void)
{
	struct cm_comp *c = sa_comp_get(cos_compid());

	assert(c);

	return c;
}

static struct cm_comp *
cm_comp_self_alloc(char *name)
{
	struct cm_comp *c = sa_comp_alloc_at_index(cos_compid());

	if (!c) return NULL;
	if (crt_booter_create(&c->comp, name, cos_compid(), 0)) BUG();

	return c;
}

static struct cm_comp *
cm_comp_alloc_with(char *name, compid_t id, struct crt_comp_resources *resources)
{
	struct cm_comp *c = sa_comp_alloc_at_index(id);

	if (!c) return NULL;
	if (crt_comp_create_with(&c->comp, name, id, resources)) {
		sa_comp_free(c);
		return NULL;
	}
	sa_comp_activate(c);

	return c;
}


struct cm_rcv *
cm_rcv_alloc_in(struct crt_comp *c, struct crt_rcv *sched, thdclosure_index_t closure_id, crt_rcv_flags_t flags)
{
	struct cm_rcv *r = sa_rcv_alloc();

	if (!r) return NULL;
	if (crt_rcv_create_in(&r->rcv, c, sched, closure_id, flags)) {
		sa_rcv_free(r);
		return NULL;
	}
	sa_rcv_activate(r);

	return r;
}

struct mm_page *
mm_page_alloc(struct cm_comp *c)
{
	struct mm_mapping *m;
	struct mm_page    *ret = NULL, *p;
	int    i;

	p = sa_page_alloc();
	if (!p) return NULL;

	m = &p->mappings[0];
	assert(m->addr == 0);

	/* Allocate page, map page */
	p->page = crt_page_allocn(&c->comp, 1);
	if (!p->page) ERR_THROW(NULL, free_p);
	if (crt_page_aliasn_in(p->page, 1, &cm_self()->comp, &c->comp, &m->addr)) BUG();

	ret = p;
done:
	return ret;
free_p:
	sa_page_free(p);
	goto done;
}

static compid_t
capmgr_comp_sched_hier_get(compid_t cid)
{
#define SCHED_STR_SZ 36 /* base-10 32 bit int + "sched_hierarchy/" */
	char *sched;
	char serialized[SCHED_STR_SZ];

	snprintf(serialized, SCHED_STR_SZ, "scheduler_hierarchy/%ld", cid);
	sched = args_get(serialized);
	if (!sched) return 0;

	return atoi(sched);
}

static compid_t
capmgr_comp_sched_get(compid_t cid)
{
#define INIT_STR_SZ 35 /* base-10 32 bit int + "init_hierarchy/" */
	char *sched;
	char serialized[SCHED_STR_SZ];

	snprintf(serialized, SCHED_STR_SZ, "init_hierarchy/%ld", cid);
	sched = args_get(serialized);
	if (!sched) return 0;

	return atoi(sched);
}

static void
capmgr_comp_init(void)
{
	struct initargs cap_entries, curr;
	struct initargs_iter i;
	vaddr_t vasfr = 0;
	capid_t capfr = 0;
	int ret, cont;
	struct cm_comp *comp;

	int remaining = 0;
	int num_comps = 0;

	/* ...then those that we're responsible for... */
	ret = args_get_entry("captbl", &cap_entries);
	assert(!ret);
	printc("Capmgr: processing %d capabilities for components that have already been booted\n", args_len(&cap_entries));

	for (cont = args_iter(&cap_entries, &i, &curr) ; cont ; ) {
		compid_t sched_id;
		compid_t id = 0;
		struct crt_comp_resources comp_res = { 0 };
		int keylen;
		int j;
		char id_serialized[16]; 	/* serialization of the id number */
		char *name;

		for (j = 0 ; j < 3 ; j++, cont = args_iter_next(&i, &curr)) {
			capid_t capid = atoi(args_key(&curr, &keylen));
			char   *type  = args_get_from("type", &curr);

			assert(capid && type);

			if (j == 0) id = atoi(args_get_from("target", &curr));
			else        assert((compid_t)atoi(args_get_from("target", &curr)) == id);

			printc("%s\n", type);

			if (!strcmp(type, "comp")) {
				comp_res.compc = capid;
			} else if (!strcmp(type, "captbl")) {
				comp_res.ctc = capid;
			} else if (!strcmp(type, "pgtbl")) {
				comp_res.ptc = capid;
			} else {
				BUG();
			}
		}

		assert(comp_res.compc && comp_res.ctc && comp_res.ptc);
		sched_id = capmgr_comp_sched_get(id);
		if (sched_id == 0) {
			sched_id = capmgr_comp_sched_hier_get(id);
		}
		assert(sched_id > 0);
		comp_res.heap_ptr        = addr_get(id, ADDR_HEAP_FRONTIER);
		comp_res.captbl_frontier = addr_get(id, ADDR_CAPTBL_FRONTIER);

		snprintf(id_serialized, 20, "names/%ld", id);
		name = args_get(id_serialized);
		assert(name);
		printc("\tCreating component %s: ", name);
		printc("id %ld, caps (captbl:%ld, pgtbl:%ld,comp:%ld)",
		       id, comp_res.ctc, comp_res.ptc, comp_res.compc);
		printc(", captbl frontier %d, heap pointer %ld, scheduler %ld\n",
		       comp_res.captbl_frontier, comp_res.heap_ptr, sched_id);
		comp = cm_comp_alloc_with(name, id, &comp_res);
		assert(comp);
	}
	assert(0);
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);
	int ret;

	printc("Starting the capability manager.\n");
	printc("\tCPU cycles per sec: %u\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));
	assert(atol(args_get("captbl_end")) >= BOOT_CAPTBL_FREE);

	/* Get our house in order. Initialize ourself and our data-structures */
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
	/*
	 * FIXME: this is a hack. The captbl_end variable does *not*
	 * take into account the synchronous invocations yet. That is
	 * because we don't want to modify the image to include it
	 * after we've sealed in all initargs and sinvs. Regardless,
	 * that is the *correct* approach.
	 */
	cos_comp_capfrontier_update(ci, addr_get(cos_compid(), ADDR_CAPTBL_FRONTIER));
	if (!cm_comp_self_alloc("capmgr")) BUG();
	sl_init(SL_MIN_PERIOD_US);

	/* Initialize the other component's for which we're responsible */
	capmgr_comp_init();

	return;
}

void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	if (init_core) return;

	cos_defcompinfo_sched_init();
	sl_init(SL_MIN_PERIOD_US);
	assert(0);
}

thdcap_t  capmgr_initthd_create(spdid_t child, thdid_t *tid) { return 0; }
thdcap_t  capmgr_initaep_create(spdid_t child, struct cos_aep_info *aep, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, asndcap_t *sndret) { return 0; }
thdcap_t capmgr_thd_create_thunk(thdclosure_index_t idx, thdid_t *tid) { return 0; }
thdcap_t  capmgr_aep_create_thunk(struct cos_aep_info *a, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax) { return 0; }
thdcap_t  capmgr_thd_create_ext(spdid_t child, thdclosure_index_t idx, thdid_t *tid) { return 0; }
thdcap_t  capmgr_aep_create_ext(spdid_t child, struct cos_aep_info *a, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, arcvcap_t *extrcv) { return 0; }
arcvcap_t capmgr_rcv_create(spdid_t child, thdid_t tid, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax) { return 0; }
asndcap_t capmgr_asnd_create(spdid_t child, thdid_t t) { return 0; }
asndcap_t capmgr_asnd_rcv_create(arcvcap_t rcv) { return 0; }
asndcap_t capmgr_asnd_key_create(cos_channelkey_t key) { return 0; }

vaddr_t memmgr_heap_page_allocn(unsigned long num_pages) { return 0; }
cbuf_t memmgr_shared_page_allocn(unsigned long num_pages, vaddr_t *pgaddr) { return 0; }
unsigned long memmgr_shared_page_map(cbuf_t id, vaddr_t *pgaddr) { return 0; }

void capmgr_create_noop(void) { return; }
