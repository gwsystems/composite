/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2020, The George Washington University
 * Author: Gabe Parmer, gparmer@gwu.edu
 */

#include <cos_debug.h>
#include <consts.h>
#include <static_slab.h>
#include <crt.h>

#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <initargs.h>
#include <addr.h>
#include <contigmem.h>

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

	struct cm_comp *client; /* component thread begins execution in */
	struct cm_comp *sched;	/* The scheduler that has the alias */
	thdcap_t aliased_cap;	/* location thread is aliased into the scheduler. */
};

struct cm_asnd {
	struct crt_asnd asnd;
};

/*
 * Shared memory should be manager -> client, and between
 * point-to-point channel recipients
 */
#define MM_MAPPINGS_MAX 5

struct mm_mapping {
	SS_STATE_T(struct cm_comp *) comp;
	vaddr_t     addr;
};

typedef unsigned int cbuf_t;
struct mm_page {
	void *page;
	struct mm_mapping mappings[MM_MAPPINGS_MAX];
};

/* Span of pages, indexed by cbuf_t */
struct mm_span {
	unsigned int page_off;
	unsigned int n_pages;
};

SS_STATIC_SLAB(comp, struct cm_comp, MAX_NUM_COMPS);
SS_STATIC_SLAB(thd, struct cm_thd, MAX_NUM_THREADS);
/* These size values are somewhat arbitrarily chosen */
SS_STATIC_SLAB(rcv, struct cm_rcv, MAX_NUM_THREADS);
SS_STATIC_SLAB(asnd, struct cm_asnd, MAX_NUM_THREADS);

/* 64 MiB */
#define MB2PAGES(mb) (round_up_to_page(mb * 1024 * 1024) / PAGE_SIZE)
#define MM_NPAGES (MB2PAGES(256))
SS_STATIC_SLAB(page, struct mm_page, MM_NPAGES);
SS_STATIC_SLAB(span, struct mm_span, MM_NPAGES);

static struct cm_comp *
cm_self(void)
{
	struct cm_comp *c = ss_comp_get(cos_compid());

	assert(c);

	return c;
}

static struct cm_comp *
cm_comp_self_alloc(char *name)
{
	struct cm_comp *c = ss_comp_alloc_at_id(cos_compid());

	assert(c);
	if (crt_booter_create(&c->comp, name, cos_compid(), 0)) BUG();
	ss_comp_activate(c);

	return c;
}

static struct cm_comp *
cm_comp_alloc_with(char *name, compid_t id, struct crt_comp_resources *resources)
{
	struct cm_comp *c = ss_comp_alloc_at_id(id);

	if (!c) return NULL;
	if (crt_comp_create_with(&c->comp, name, id, resources)) {
		ss_comp_free(c);
		return NULL;
	}
	ss_comp_activate(c);

	return c;
}

struct cm_rcv *
cm_rcv_alloc_in(struct crt_comp *c, struct crt_rcv *sched, thdclosure_index_t closure_id, crt_rcv_flags_t flags)
{
	struct cm_rcv *r = ss_rcv_alloc();

	if (!r) return NULL;
	if (crt_rcv_create_in(&r->rcv, c, sched, closure_id, flags)) {
		ss_rcv_free(r);
		return NULL;
	}
	ss_rcv_activate(r);

	return r;
}

struct cm_thd *
cm_thd_alloc_in(struct cm_comp *c, struct cm_comp *sched, thdclosure_index_t closure_id)
{
	struct cm_thd *t = ss_thd_alloc();
	struct crt_thd_resources res = { 0 };

	if (!t) return NULL;
	if (crt_thd_create_in(&t->thd, &c->comp, closure_id)) {
		ss_thd_free(t);
		printc("capmgr: couldn't create new thread correctly.\n");
		return NULL;
	}
	ss_thd_activate(t);
	if (crt_thd_alias_in(&t->thd, &sched->comp, &res)) {
		printc("capmgr: couldn't alias correctly.\n");
		/* FIXME: reclaim the thread */
		return NULL;
	}
	t->sched       = sched;
	t->aliased_cap = res.cap;
	t->client      = c;
	/* FIXME: should take a reference to the scheduler */

	return t;
}

/**
 * Allocate a page from the pool of physical memory into a component.
 *
 * - @c - The component to allocate into.
 * - @return - the allocated and initialized page, or `NULL` if no
 *   page is available.
 */
static struct mm_page *
mm_page_alloc(struct cm_comp *c, unsigned long align)
{
	struct mm_mapping *m;
	struct mm_page    *ret = NULL, *p;
	int    i;

	p = ss_page_alloc();
	if (!p) return NULL;

	m = &p->mappings[0];
	if (ss_state_alloc(&m->comp)) BUG();

	/* Allocate page, map page */
	p->page = crt_page_allocn(&cm_self()->comp, 1);
	if (!p->page) ERR_THROW(NULL, free_p);
	if (crt_page_aliasn_aligned_in(p->page, align, 1, &cm_self()->comp, &c->comp, &m->addr)) BUG();

	ss_state_activate_with(&m->comp, (word_t)c);
	ss_page_activate(p);
	ret = p;
done:
	return ret;
free_p:
	ss_page_free(p);
	goto done;
}

static struct mm_page *
mm_page_allocn(struct cm_comp *c, unsigned long num_pages, unsigned long align)
{
	struct mm_page *p, *prev, *initial;
	unsigned long i;

	initial = prev = p = mm_page_alloc(c, align);
	if (!p) return 0;
	for (i = 1; i < num_pages; i++) {
		p = mm_page_alloc(c, PAGE_SIZE);
		if (!p) return NULL;
		if ((prev->page + 4096) != p->page) {
			BUG(); /* FIXME: handle concurrency */
		}
		prev = p;
	}

	return initial;
}

vaddr_t
memmgr_virt_to_phys(vaddr_t vaddr)
{
	struct cm_comp *c;

	c = ss_comp_get(cos_inv_token());
	if (!c) return 0;

	return call_cap_op(c->comp.comp_res->ci.pgtbl_cap, CAPTBL_OP_INTROSPECT, (vaddr_t)vaddr, 0, 0, 0);
}

static void
contigmem_check(vaddr_t vaddr, int npages)
{
	vaddr_t paddr_pre = 0, paddr_next = 0;

	paddr_pre = memmgr_virt_to_phys(vaddr);

	for (int i = 1; i < npages; i++) {
		paddr_next = memmgr_virt_to_phys(vaddr + i * PAGE_SIZE);
		assert(paddr_next - paddr_pre == PAGE_SIZE);

		paddr_pre = paddr_next;
	}
}

vaddr_t
contigmem_alloc(unsigned long npages)
{
	struct cm_comp *c;
	struct mm_page *p;

	struct mm_mapping *m;

	vaddr_t vaddr;
	unsigned long i;

	c = ss_comp_get(cos_inv_token());
	if (!c) return 0;

	void *page = crt_page_allocn(&cm_self()->comp, npages);
	assert(page);

	if (crt_page_aliasn_aligned_in(page, PAGE_SIZE, npages, &cm_self()->comp, &c->comp, &vaddr)) BUG();

	for (i = 0; i < npages; i++) {
		p = ss_page_alloc();
		assert(p);

		m = &p->mappings[0];
		if (ss_state_alloc(&m->comp)) BUG();

		p->page = page + i * PAGE_SIZE;
		m->addr = vaddr + i * PAGE_SIZE;

		ss_state_activate_with(&m->comp, (word_t)c);
		ss_page_activate(p);
	}

	contigmem_check((vaddr_t)vaddr, npages);
	return vaddr;
}

cbuf_t
contigmem_shared_alloc_aligned(unsigned long npages, unsigned long align, vaddr_t *pgaddr)
{
	struct cm_comp *c;
	struct mm_page *p, *initial = NULL;
	struct mm_span *s;

	struct mm_mapping *m;

	vaddr_t vaddr;

	unsigned long i;
	int ret;

	c = ss_comp_get(cos_inv_token());
	if (!c) return 0;

	void *page = crt_page_allocn(&cm_self()->comp, npages);
	assert(page);

	if (crt_page_aliasn_aligned_in(page, align, npages, &cm_self()->comp, &c->comp, &vaddr)) BUG();

	s = ss_span_alloc();
	if (!s) return 0;
	for (i = 0; i < npages; i++) {
		p = ss_page_alloc();
		assert(p);

		if (unlikely(i == 0)) {
			initial = p;
		}

		m = &p->mappings[0];
		if (ss_state_alloc(&m->comp)) BUG();

		p->page = page + i * PAGE_SIZE;
		m->addr = vaddr + i * PAGE_SIZE;

		ss_state_activate_with(&m->comp, (word_t)c);
		ss_page_activate(p);
	}

	/**
	 * FIXME: Need to reslove concurrent issue here,
	 * this is not multi-thread safe
	 */
	s->page_off = ss_page_id(initial);
	s->n_pages  = npages;
	ss_span_activate(s);

	ret = ss_span_id(s);

	*pgaddr = initial->mappings[0].addr;

	contigmem_check((vaddr_t)vaddr, npages);
	return ret;
}

vaddr_t
memmgr_heap_page_allocn_aligned(unsigned long num_pages, unsigned long align)
{
	struct cm_comp *c;
	struct mm_page *p;

	c = ss_comp_get(cos_inv_token());
	if (!c) return 0;
	p = mm_page_allocn(c, num_pages, align);
	if (!p) return 0;

	return (vaddr_t)p->mappings[0].addr;
}

vaddr_t
memmgr_map_phys_to_virt(paddr_t paddr, size_t size)
{
	struct cm_comp *c;

	c = ss_comp_get(cos_inv_token());
	if (!c) return 0;

	return (vaddr_t)cos_hw_map(&c->comp.comp_res->ci, BOOT_CAPTBL_SELF_INITHW_BASE, paddr, size);
}

vaddr_t
memmgr_heap_page_allocn(unsigned long num_pages)
{
	return memmgr_heap_page_allocn_aligned(num_pages, PAGE_SIZE);
}

cbuf_t
memmgr_shared_page_allocn_aligned(unsigned long num_pages, unsigned long align, vaddr_t *pgaddr)
{
	struct cm_comp *c;
	struct mm_page *p;
	struct mm_span *s;
	cbuf_t ret = 0;

	c = ss_comp_get(cos_inv_token());
	if (!c) return 0;
	s = ss_span_alloc();
	if (!s) return 0;
	p = mm_page_allocn(c, num_pages, align);
	if (!p) ERR_THROW(0, cleanup);

	s->page_off = ss_page_id(p);
	s->n_pages  = num_pages;
	ss_span_activate(s);

	ret = ss_span_id(s);

	*pgaddr = p->mappings[0].addr;
done:
	return ret;
cleanup:
	ss_span_free(s);
	goto done;
}

cbuf_t
memmgr_shared_page_allocn(unsigned long num_pages, vaddr_t *pgaddr)
{
	return memmgr_shared_page_allocn_aligned(num_pages, PAGE_SIZE, pgaddr);
}


/**
 * Alias a page of memory into another component (i.e., create shared
 * memory). The number of mappings are limited by `MM_MAPPINGS_MAX`.
 *
 * @p - the page we're going to alias
 * @c - the component to alias into
 * @addr - returns the virtual address mapped into
 * @return - `0` = success, `<0` = error
 */
static int
mm_page_alias(struct mm_page *p, struct cm_comp *c, vaddr_t *addr, unsigned long align)
{
	struct mm_mapping *m;
	int i;

	*addr = 0;
	for (i = 1; i < MM_MAPPINGS_MAX; i++) {
		m = &p->mappings[i];

		if (ss_state_alloc(&m->comp)) continue;
		if (crt_page_aliasn_aligned_in(p->page, align, 1, &cm_self()->comp, &c->comp, &m->addr)) BUG();
		assert(m->addr);
		*addr = m->addr;
		ss_state_activate_with(&m->comp, (word_t)c);

		return 0;
	}
	assert(i == MM_MAPPINGS_MAX);

	return -ENOMEM;
}

unsigned long
memmgr_shared_page_map_aligned(cbuf_t id, unsigned long align, vaddr_t *pgaddr)
{
	struct cm_comp *c;
	struct mm_span *s;
	struct mm_page *p;
	unsigned int i;
	vaddr_t addr;

	*pgaddr = 0;
	s = ss_span_get(id);
	if (!s) return 0;
	c = ss_comp_get(cos_inv_token());
	if (!c) return 0;

	for (i = 0; i < s->n_pages; i++) {
		struct mm_page *p;

		p = ss_page_get(s->page_off + i);
		if (!p) return 0;

		if (mm_page_alias(p, c, &addr, align)) BUG();
		if (*pgaddr == 0) *pgaddr = addr;
		align = PAGE_SIZE; // only the first page can have special alignment
	}

	return s->n_pages;
}

unsigned long
memmgr_shared_page_map(cbuf_t id, vaddr_t *pgaddr)
{
	return memmgr_shared_page_map_aligned(id, PAGE_SIZE, pgaddr);
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
	struct initargs cap_entries, exec_entries, curr;
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
		assert(id);

		assert(comp_res.compc && comp_res.ctc && comp_res.ptc);
		sched_id = capmgr_comp_sched_get(id);
		if (sched_id == 0) {
			sched_id = capmgr_comp_sched_hier_get(id);
		}
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

	/* Create execution in the relevant components */
	ret = args_get_entry("execute", &exec_entries);
	assert(!ret);
	printc("Capmgr: %d components that need execution\n", args_len(&exec_entries));
	for (cont = args_iter(&exec_entries, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		struct cm_comp    *cmc;
		struct crt_comp   *comp;
		int      keylen;
		compid_t id        = atoi(args_key(&curr, &keylen));
		char    *exec_type = args_value(&curr);
		struct crt_comp_exec_context ctxt = { 0 };

		assert(exec_type);
		assert(id != cos_compid());
		cmc  = ss_comp_get(id);
		assert(cmc);
		comp = &cmc->comp;

		if (!strcmp(exec_type, "sched")) {
			struct cm_rcv *r = ss_rcv_alloc();

			assert(r);
			if (crt_comp_exec(comp, crt_comp_exec_sched_init(&ctxt, &r->rcv))) BUG();
			ss_rcv_activate(r);
			printc("\tCreated scheduling execution for %ld\n", id);
		} else if (!strcmp(exec_type, "init")) {
			struct cm_thd *t = ss_thd_alloc();

			assert(t);
			if (crt_comp_exec(comp, crt_comp_exec_thd_init(&ctxt, &t->thd))) BUG();
			ss_thd_activate(t);
			printc("\tCreated thread for %ld\n", id);
		} else {
			printc("Error: Found unknown execution schedule type %s.\n", exec_type);
			BUG();
		}
	}

	return;
}

static inline struct crt_comp *
crtcomp_get(compid_t id)
{
	struct cm_comp *c = ss_comp_get(id);

	assert(c);

	return &c->comp;
}

void
execute(void)
{
	crt_compinit_execute(crtcomp_get);
}

void
capmgr_set_tls(thdcap_t cap, void* tls_addr)
{
	compid_t cid = (compid_t)cos_inv_token();
	struct crt_comp* c = crtcomp_get(cid);

	cos_thd_mod(&c->comp_res->ci, cap, tls_addr);
}

void
init_done(int parallel_init, init_main_t main_type)
{
	compid_t client = (compid_t)cos_inv_token();
	struct crt_comp *c;

	assert(client > 0 && client <= MAX_NUM_COMPS);
	c = crtcomp_get(client);

	crt_compinit_done(c, parallel_init, main_type);

	return;
}


void
init_exit(int retval)
{
	compid_t client = (compid_t)cos_inv_token();
	struct crt_comp *c;

	assert(client > 0 && client <= MAX_NUM_COMPS);
	c = crtcomp_get(client);
	assert(c);

	crt_compinit_exit(c, retval);

	while (1) ;
}

thdcap_t
capmgr_thd_create_ext(spdid_t client, thdclosure_index_t idx, thdid_t *tid)
{
	compid_t schedid = (compid_t)cos_inv_token();
	struct cm_thd *t;
	struct cm_comp *s, *c;

	if (schedid != capmgr_comp_sched_get(client)) {
		/* don't have permission to create execution in that component. */
		printc("capmgr: Component asking to create thread from %ld in %ld -- no permission.\n",
		       schedid, (compid_t)client);
		return 0;
	}

	c = ss_comp_get(client);
	s = ss_comp_get(schedid);
	if (!c || !s) return 0;
	t = cm_thd_alloc_in(c, s, idx);
	if (!t) {
		/* TODO: release resources */
		return 0;
	}
	*tid = t->thd.tid;

	return t->aliased_cap;
}

thdcap_t
capmgr_initthd_create(spdid_t client, thdid_t *tid)
{
	return capmgr_thd_create_ext(client, 0, tid);
}

thdcap_t  capmgr_initaep_create(spdid_t child, struct cos_aep_info *aep, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, asndcap_t *sndret) { BUG(); return 0; }

thdcap_t
capmgr_thd_create_thunk(thdclosure_index_t idx, thdid_t *tid)
{
	compid_t client = (compid_t)cos_inv_token();
	struct cm_thd *t;
	struct cm_comp *c;

	assert(client > 0 && client <= MAX_NUM_COMPS);
	c = ss_comp_get(client);
	t = cm_thd_alloc_in(c, c, idx);
	if (!t) {
		/* TODO: release resources */
		return 0;
	}
	*tid = t->thd.tid;

	return t->aliased_cap;
}

thdcap_t  capmgr_aep_create_thunk(struct cos_aep_info *a, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax) { BUG(); return 0; }
thdcap_t  capmgr_aep_create_ext(spdid_t child, struct cos_aep_info *a, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, arcvcap_t *extrcv) { BUG(); return 0; }
arcvcap_t capmgr_rcv_create(spdid_t child, thdid_t tid, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax) { BUG(); return 0; }
asndcap_t capmgr_asnd_create(spdid_t child, thdid_t t) { BUG(); return 0; }
asndcap_t capmgr_asnd_rcv_create(arcvcap_t rcv) { BUG(); return 0; }
asndcap_t capmgr_asnd_key_create(cos_channelkey_t key) { BUG(); return 0; }

void capmgr_create_noop(void) { return; }

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);
	struct initargs curr, comps;
	struct initargs_iter i;
	int cont, found_shared = 0;
	int ret;

	printc("Starting the capability manager.\n");
	assert(atol(args_get("captbl_end")) >= BOOT_CAPTBL_FREE);

	/* Example code to walk through the components in shared address spaces */
	printc("Components in shared address spaces: ");
	ret = args_get_entry("addrspc_shared", &comps);
	assert(!ret);
	for (cont = args_iter(&comps, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		compid_t id = atoi(args_value(&curr));

		found_shared = 1;
		printc("%ld ", id);
	}
	if (!found_shared) {
		printc("none");
	}
	printc("\n");

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

	/* Initialize the other component's for which we're responsible */
	capmgr_comp_init();

	sl_init(SL_MIN_PERIOD_US);

	return;
}

void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	if (!init_core) cos_defcompinfo_sched_init();
}

void
parallel_main(coreid_t cid)
{
	execute();
	sl_init(SL_MIN_PERIOD_US);
}
