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
#include <initargs.h>
#include <addr.h>
#include <dcb.h>
#include <contigmem.h>

struct cm_rcv {
	struct crt_rcv rcv;
	struct cm_comp *sched;
};

struct cm_comp {
	struct crt_comp comp;
	struct cm_rcv *sched_rcv;    /* rcv cap for this scheduler or NULL if not a scheduler */
	struct cm_rcv *sched_parent; /* rcv cap for this scheduler's scheduler, or NULL */
	vaddr_t dcb_init_ptr;
};

struct cm_thd {
	struct crt_thd  thd;

	struct cm_comp *client; /* component thread begins execution in */
	struct cm_comp *sched;	/* The scheduler that has the alias */
	thdcap_t aliased_cap;	/* location thread is aliased into the scheduler. */
};

struct cm_asnd {
	struct crt_asnd asnd;
};

struct cm_dcb {
	dcbcap_t dcb_cap;
	vaddr_t  dcb_addr;
	dcboff_t dcb_off;
};

struct cm_dcbinfo {
	struct cm_dcb *dcb;
	arcvcap_t      arcv;
	asndcap_t      asnd;
	thdid_t        tid;
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

#define MAX_DCB_NUM PAGE_SIZE/sizeof(struct cos_dcb_info)
SS_STATIC_SLAB(dcb, struct cm_dcb, MAX_DCB_NUM);
SS_STATIC_SLAB(dcbinfo, struct cm_dcbinfo, MAX_NUM_THREADS);

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
	c->dcb_init_ptr = resources->heap_ptr - (NUM_CPU * PAGE_SIZE);
	ss_comp_activate(c);

	return c;
}

struct cm_rcv *
cm_rcv_alloc_in(struct crt_comp *c, struct crt_rcv *sched, thdclosure_index_t closure_id, crt_rcv_flags_t flags)
{
	struct cm_rcv *r = ss_rcv_alloc();
	vaddr_t        dcbaddr;

	if (!r) return NULL;
	if (crt_rcv_create_in(&r->rcv, c, sched, closure_id, flags, &dcbaddr)) {
		ss_rcv_free(r);
		return NULL;
	}
	ss_rcv_activate(r);

	return r;
}

struct cm_thd *
cm_thd_alloc_in(struct cm_comp *c, struct cm_comp *sched, struct cm_dcb *dcb, thdclosure_index_t closure_id, arcvcap_t *arcv, asndcap_t *asnd)
{
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci        = cos_compinfo_get(defci);
	struct cos_compinfo    *sched_ci  = cos_compinfo_get(sched->comp.comp_res);
	struct cos_aep_info    *sched_aep;

	struct cm_thd  *t = ss_thd_alloc();
	struct cm_rcv  *r = ss_rcv_alloc();
	struct cm_asnd *s = ss_asnd_alloc();

	struct crt_rcv_resources _rcv   = (struct crt_rcv_resources) { 0 };
	struct crt_asnd_resources _asnd = (struct crt_asnd_resources) { 0 };

	tcap_t    tcap;
	arcvcap_t rcvcap;

	if (!t || !r || !s) return NULL;
	if (crt_thd_create_in(&t->thd, &c->comp, dcb->dcb_cap, dcb->dcb_off, closure_id)) {
		ss_thd_free(t);
		printc("capmgr: couldn't create new thread correctly.\n");
		return NULL;
	}
	ss_thd_activate(t);

	sched_aep = cos_sched_aep_get(sched->comp.comp_res);
	assert(sched_ci->comp_cap);

	//tcap = cos_tcap_alloc(ci);
	//assert(tcap);
	rcvcap = cos_arcv_alloc(ci, t->thd.cap, sched_aep->tc, sched_ci->comp_cap, sched_aep->rcv);
	assert(rcvcap);

	struct crt_rcv_resources resrcv = (struct crt_rcv_resources) {
		.tc = sched_aep->tc,
		.thd = t->thd.cap,
		.tid = 0,
		.rcv = rcvcap,
	};
	if (crt_rcv_create_with(&r->rcv, &sched->comp, &resrcv)) BUG();
	ss_rcv_activate(r);

	if (crt_rcv_alias_in(&r->rcv, &sched->comp, &_rcv, CRT_RCV_ALIAS_THD | CRT_RCV_ALIAS_RCV | CRT_RCV_ALIAS_TCAP)) BUG();

	if (crt_asnd_create(&s->asnd, &r->rcv)) BUG();
	ss_asnd_activate(s);
	if (crt_asnd_alias_in(&s->asnd, &sched->comp, &_asnd)) BUG();

	t->sched       = sched;
	t->aliased_cap = _rcv.thd;
	t->client      = c;
	*arcv          = _rcv.rcv;
	//printc("[capmgr]tid: %d, rcv: %d, asnd: %d, dcb: %x\n", t->thd.tid, _rcv.rcv, _asnd.asnd, dcb->dcb_addr);
	*asnd          = _asnd.asnd;
	/* FIXME: should take a reference to the scheduler */

	return t;
}

struct cm_dcb *
cm_dcb_alloc_in(struct cm_comp *sched)
{
	compid_t       id = (compid_t)cos_inv_token();
	struct cm_dcb *d  = ss_dcb_alloc();
	dcbcap_t       dcbcap = 0;
	vaddr_t        dcbaddr = 0;
	dcboff_t       dcboff = 0;

	//dcbcap = crt_dcb_create_in(&sched->comp, &dcbaddr);
	dcbcap = cos_dcb_info_alloc(&sched->comp.dcb_data[cos_cpuid()], &dcboff, &dcbaddr);
	assert(dcbcap);
	d->dcb_addr = dcbaddr;
	d->dcb_cap  = dcbcap;
	d->dcb_off  = dcboff;
	ss_dcb_activate(d);

	return d;
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

extern scbcap_t scb_mapping(compid_t id, vaddr_t scb_uaddr);

int
capmgr_scb_mapping(void)
{
	compid_t schedid = (compid_t)cos_inv_token();
	struct cos_compinfo *ci;
	struct cm_comp *s;
	struct cos_defcompinfo *def = cos_defcompinfo_curr_get();
	vaddr_t scb_uaddr;

	if (schedid == 0) {
		ci = cos_compinfo_get(def);
		scb_uaddr = (vaddr_t)cos_scb_info_get();
	} else {
		s = ss_comp_get(schedid);
		assert(s);

		ci = cos_compinfo_get(s->comp.comp_res);
		scb_uaddr = (vaddr_t)(s->dcb_init_ptr - COS_SCB_SIZE);
		//printc("dcbstart: %x\n", s->dcb_init_ptr);
	}
	assert(ci);

	//scb_uaddr = cos_page_bump_intern_valloc(ci, COS_SCB_SIZE);
	//printc("scb: %d,%x\n", schedid, scb_uaddr);
	assert(scb_uaddr);

	return scb_mapping(schedid, scb_uaddr);
}

static void
capmgr_dcb_info_init(struct cm_comp *c)
{
	struct crt_comp        *comp      = &c->comp;
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci        = cos_compinfo_get(defci);
	struct cos_compinfo    *target_ci = cos_compinfo_get(comp->comp_res);
	dcbcap_t initdcb  = 0;
	vaddr_t  initaddr = 0;
	dcboff_t initoff  = 0;

	//initaddr = cos_page_bump_intern_valloc(target_ci, PAGE_SIZE);
	//printc("dcbaddr: %x, target_ci: %x\n", initaddr, target_ci);
	initaddr = c->dcb_init_ptr + (cos_cpuid() * PAGE_SIZE);
	//printc("dcbaddr: %x, target_ci: %x, %x\n", initaddr, target_ci, initaddr+PAGE_SIZE);
	assert(initaddr);

	initdcb  = cos_dcb_alloc(ci, target_ci->pgtbl_cap, initaddr);
	assert(initdcb);
	cos_dcb_info_init_ext(&comp->dcb_data[cos_cpuid()], target_ci, initdcb, initaddr, initoff);

	return;
}

static void
capmgr_execution_init(int is_init_core)
{
	struct initargs cap_entries, exec_entries, curr;
	struct initargs_iter i;
	vaddr_t vasfr = 0;
	capid_t capfr = 0;
	int ret, cont;

	/* Create execution in the relevant components */
	ret = args_get_entry("execute", &exec_entries);
	assert(!ret);
	if (is_init_core) printc("Capmgr: %d components that need execution\n", args_len(&exec_entries));
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
			//if (is_init_core) capmgr_dcb_info_init(cmc);
			capmgr_dcb_info_init(cmc);
			if (crt_comp_exec(comp, crt_comp_exec_sched_init(&ctxt, &r->rcv))) BUG();
			ss_rcv_activate(r);
			cmc->sched_rcv = r;
			if (is_init_core) printc("\tCreated scheduling execution for %ld\n", id);
		} else if (!strcmp(exec_type, "init")) {
			struct cm_thd *t = ss_thd_alloc();
			assert(t);
			if (crt_comp_exec(comp, crt_comp_exec_thd_init(&ctxt, &t->thd))) BUG();
			ss_thd_activate(t);
			if (is_init_core) printc("\tCreated thread for %ld\n", id);
		} else {
			printc("Error: Found unknown execution schedule type %s.\n", exec_type);
			BUG();
		}
	}

	return;
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
		comp_res.heap_ptr        = addr_get(id, ADDR_HEAP_FRONTIER) + COS_SCB_SIZE + (PAGE_SIZE * NUM_CPU);
		comp_res.captbl_frontier = addr_get(id, ADDR_CAPTBL_FRONTIER);

		snprintf(id_serialized, 20, "names/%ld", id);
		name = args_get(id_serialized);
		assert(name);
		printc("\tCreating component %s: id %ld\n", name, id);
		printc("\t\tcaptbl:%ld, pgtbl:%ld, comp:%ld, captbl/pgtbl frontiers %d & %lx, sched %ld\n",
		       comp_res.ctc, comp_res.ptc, comp_res.compc, comp_res.captbl_frontier, comp_res.heap_ptr, sched_id);
		comp = cm_comp_alloc_with(name, id, &comp_res);
		assert(comp);
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

vaddr_t
capmgr_sched_initdcb_get()
{
	compid_t schedid = (compid_t)cos_inv_token();

	struct cm_comp *s;
	s = ss_comp_get(schedid);

	return s->comp.init_dcb_addr[cos_cpuid()];
}

thdid_t
capmgr_retrieve_dcbinfo(thdid_t tid, arcvcap_t *arcv, asndcap_t *asnd, struct cos_dcb_info **dcb)
{
	struct cm_dcbinfo *info;

	info = ss_dcbinfo_get(tid);
	//printc("---------tid: %d, info: %x, ret: %d, dcbaddr: %x\n", tid, info, info->tid, info->dcb->dcb_addr);
	*arcv = info->arcv;
	*asnd = info->asnd;
	*dcb  = (struct cos_dcb_info *)info->dcb->dcb_addr;

	return info->tid;
}

thdcap_t
capmgr_thd_create_ext(spdid_t client, thdclosure_index_t idx, thdid_t *tid)
{
	compid_t schedid = (compid_t)cos_inv_token();
	struct cm_thd *t;
	struct cm_comp *s, *c;
	struct cm_dcb *d;
	struct cm_dcbinfo *info;
	asndcap_t _asnd;
	arcvcap_t _arcv;

	if (schedid != capmgr_comp_sched_get(client)) {
		/* don't have permission to create execution in that component. */
		printc("capmgr: Component asking to create thread from %ld in %ld -- no permission.\n",
		       schedid, (compid_t)client);
		return 0;
	}

	c = ss_comp_get(client);
	s = ss_comp_get(schedid);

	if (!c || !s) return 0;

	d = cm_dcb_alloc_in(s);
	assert(d);

	t = cm_thd_alloc_in(c, s, d, idx, &_arcv, &_asnd);
	if (!t) {
		/* TODO: release resources */
		return 0;
	}

	info = ss_dcbinfo_alloc_at_id(t->thd.tid);
	info->arcv    = _arcv;
	info->asnd    = _asnd;
	info->dcb     = d;
	info->tid     = t->thd.tid;
	ss_dcbinfo_activate(info);

	*tid  = t->thd.tid;
	//*arcv = t->arcv_cap;
	//*asnd = _asnd;
	//*arcv = _arcv;
	//*dcb  = (struct cos_dcb_info *)d->dcb_addr;
	//printc("*tid: %d, arcv: %d\n", t->thd.tid, _arcv);

	return t->aliased_cap;
}

thdcap_t
capmgr_initthd_create(spdid_t client, thdid_t *tid)
{
	assert(0);
	return capmgr_thd_create_ext(client, 0, tid);
}

thdcap_t  capmgr_initaep_create(spdid_t child, struct cos_aep_info *aep, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, asndcap_t *sndret) { BUG(); return 0; }

thdcap_t
capmgr_thd_create_thunk(thdclosure_index_t idx, thdid_t *tid, struct cos_dcb_info **dcb)
{
	compid_t client = (compid_t)cos_inv_token();
	struct cm_thd  *t;
	struct cm_comp *c;
	struct cm_dcb  *d;
	struct cm_dcbinfo *info;
	asndcap_t       asnd;
	arcvcap_t       arcv;

	assert(client > 0 && client <= MAX_NUM_COMPS);
	c = ss_comp_get(client);
	d = cm_dcb_alloc_in(c);
	assert(d);
	t = cm_thd_alloc_in(c, c, d, idx, &arcv, &asnd);

	if (!t) {
		/* TODO: release resources */
		assert(0);
		return 0;
	}

	info = ss_dcbinfo_alloc_at_id(t->thd.tid);
	info->arcv = arcv;
	info->asnd = asnd;

	*tid = t->thd.tid;
	*dcb = (struct cos_dcb_info *)d->dcb_addr;

	return t->aliased_cap;
}

thdcap_t  capmgr_aep_create_thunk(struct cos_aep_info *a, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, struct cos_dcb_info **dcb) { BUG(); return 0; }
thdcap_t  capmgr_aep_create_ext(spdid_t child, struct cos_aep_info *a, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, struct cos_dcb_info **dcb, arcvcap_t *extrcv) { BUG(); return 0; }

asndcap_t capmgr_asnd_create(spdid_t child, thdid_t t) { BUG(); return 0; }
arcvcap_t capmgr_rcv_create(spdid_t child, thdcap_t thdcap) { BUG(); return 0; }
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
	cos_comp_capfrontier_update(ci, addr_get(cos_compid(), ADDR_CAPTBL_FRONTIER), 0);
	if (!cm_comp_self_alloc("capmgr")) BUG();

	/* Initialize the other component's for which we're responsible */
	//scbcap_t scbc = cos_scb_alloc(ci);
	//assert(scbc);
	//vaddr_t scb_uaddr = cos_page_bump_intern_valloc(ci, COS_SCB_SIZE);
	//printc("scb_uaddr: %x, ci: %x, defci: %x\n", scb_uaddr, ci, defci);
	//printc("========> scb: %d\n", ci->pgtbl_cap);
	//if (cos_scb_mapping(ci, ci->comp_cap, ci->pgtbl_cap, scbc, scb_uaddr)) BUG();
	if (capmgr_scb_mapping()) BUG();

	capmgr_comp_init();

	return;
}

void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	cos_defcompinfo_sched_init();
	capmgr_execution_init(init_core);
}

void
parallel_main(coreid_t cid)
{
	crt_compinit_execute(crtcomp_get);
}
