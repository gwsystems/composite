/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2019, The George Washington University
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */
#include <stdlib.h>

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <cos_types.h>
#include <llprint.h>
#include <elf_loader.h>

#include <crt.h>

#define CRT_REFCNT_INITVAL 1

static inline int
crt_refcnt_alive(crt_refcnt_t *r)
{
	return *r > 0;
}

/*
 * Return 0 on success, non-zero on failure (i.e. calling this on a
 * component that is not already active)
 */
static inline void
crt_refcnt_take(crt_refcnt_t *r)
{
	assert(crt_refcnt_alive(r));
	if (ps_faa(r, 1) == 0) BUG();
}

/* return 1 if the last reference is released, 0 otherwise */
static inline int
crt_refcnt_release(crt_refcnt_t *r)
{
	assert(crt_refcnt_alive(r));
	if (ps_faa(r, -1) == 1) return 1;

	return 0;
}

/*
 * Create the component from the elf object including all the resource
 * tables, and memory.
 *
 * Notes:
 * - The capability tables are empty.
 * - `name` is *not* copied, so it is borrowed from within `c`. Copy
 *   it manually if you can't guarantee it will stay alive.
 *
 * Return 0 on success, -errno on failure -- either of elf parsing, or
 * of memory allocation.
 */
int
crt_comp_create(struct crt_comp *c, char *name, compid_t id, void *elf_hdr, vaddr_t info)
{
	size_t  ro_sz,   rw_sz, data_sz, bss_sz, tot_sz;
	char   *ro_src, *data_src, *mem;
	int     ret;
	struct cos_compinfo *ci, *root_ci;
	struct cos_component_information *comp_info;
	unsigned long info_offset;

	*c = (struct crt_comp) {
		.flags      = CRT_COMP_NONE,
		.name       = name,
		.id         = id,
		.elf_hdr    = elf_hdr,
		.entry_addr = elf_entry_addr(elf_hdr),
		.comp_res   = &c->comp_res_mem,
		.info       = info,
		.refcnt     = CRT_REFCNT_INITVAL
	};
	assert(c->entry_addr != 0);

	ci      = cos_compinfo_get(c->comp_res);
	root_ci = cos_compinfo_get(cos_defcompinfo_curr_get());

	if (elf_load_info(c->elf_hdr, &c->ro_addr, &ro_sz, &ro_src, &c->rw_addr, &data_sz, &data_src, &bss_sz)) return -EINVAL;

	printc("\t\tElf object: ro [0x%lx, 0x%lx), data [0x%lx, 0x%lx), bss [0x%lx, 0x%lx).\n",
	       c->ro_addr, c->ro_addr + ro_sz, c->rw_addr, c->rw_addr + data_sz, c->rw_addr + data_sz, c->rw_addr + data_sz + bss_sz);

	ret = cos_compinfo_alloc(ci, c->ro_addr, BOOT_CAPTBL_FREE, c->entry_addr, root_ci);
	assert(!ret);

	tot_sz = round_up_to_page(round_up_to_page(ro_sz) + data_sz + bss_sz);
	mem    = cos_page_bump_allocn(root_ci, tot_sz);
	if (!mem) return -ENOMEM;
	c->mem = mem;

	memcpy(mem, ro_src, ro_sz);
	memcpy(mem + round_up_to_page(ro_sz), data_src, data_sz);
	memset(mem + round_up_to_page(ro_sz) + data_sz, 0, bss_sz);

	assert(info >= c->rw_addr && info < c->rw_addr + data_sz);
	info_offset = info - c->rw_addr;
	comp_info   = (struct cos_component_information *)(mem + round_up_to_page(ro_sz) + info_offset);
	assert(comp_info->cos_this_spd_id == 0);
	comp_info->cos_this_spd_id = id;

	/* FIXME: separate map of RO and RW */
	if (c->ro_addr != cos_mem_aliasn(ci, root_ci, (vaddr_t)mem, tot_sz)) return -ENOMEM;

	return 0;
}

int
crt_booter_create(struct crt_comp *c, char *name, compid_t id, vaddr_t info)
{
	*c = (struct crt_comp) {
		.flags      = CRT_COMP_BOOTER,
		.name       = name,
		.id         = id,
		.comp_res   = cos_defcompinfo_curr_get(),
		.info       = info,
		.refcnt     = CRT_REFCNT_INITVAL
	};

	return 0;
}

static int
crt_is_booter(struct crt_comp *c)
{
	return c->flags & CRT_COMP_BOOTER;
}

int
crt_sinv_create(struct crt_sinv *sinv, char *name, struct crt_comp *server, struct crt_comp *client,
		vaddr_t c_fn_addr, vaddr_t c_ucap_addr, vaddr_t s_fn_addr)
{
	struct cos_compinfo *cli = cos_compinfo_get(client->comp_res);
	struct cos_compinfo *srv = cos_compinfo_get(server->comp_res);
	unsigned int ucap_off;
	struct usr_inv_cap *ucap;

	assert(crt_refcnt_alive(&server->refcnt) && crt_refcnt_alive(&client->refcnt));
	crt_refcnt_take(&client->refcnt);
	crt_refcnt_take(&server->refcnt);

	assert(cli && cli->memsrc && srv && srv->memsrc && srv->comp_cap);
	assert(!crt_is_booter(client));

	*sinv = (struct crt_sinv) {
		.name        = name,
		.server      = server,
		.client      = client,
		.c_fn_addr   = c_fn_addr,
		.c_ucap_addr = c_ucap_addr,
		.s_fn_addr   = s_fn_addr
	};

	sinv->sinv_cap = cos_sinv_alloc(cli, srv->comp_cap, sinv->s_fn_addr, client->id);
	assert(sinv->sinv_cap);

	/* poor-mans virtual address translation from client VAS -> our ptrs */
	assert(sinv->c_ucap_addr - sinv->client->ro_addr > 0);
	ucap_off = sinv->c_ucap_addr - sinv->client->ro_addr;
	ucap = (struct usr_inv_cap *)(sinv->client->mem + ucap_off);
	*ucap = (struct usr_inv_cap) {
		.invocation_fn = sinv->c_fn_addr,
		.cap_no        = sinv->sinv_cap,
		.data          = NULL
	};

	return 0;
}

/*
 * Create a new thread in the component @c in response to a request
 * to create the thread from that component (thus passing in the
 * requested @closure_id).
 */
int
crt_thd_create(struct crt_thd *t, struct crt_comp *c, thdclosure_index_t closure_id)
{
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_compinfo    *target_ci = cos_compinfo_get(c->comp_res);
	thdcap_t thdcap;

	crt_refcnt_take(&c->refcnt);
	assert(target_ci->comp_cap);
	thdcap = cos_thd_alloc_ext(cos_compinfo_get(defci), target_ci->comp_cap, closure_id);
	assert(thdcap);

	*t = (struct crt_thd) {
		.cap = thdcap,
		.c   = c
	};

	return 0;
}

/*
 * @closure_id == 0 means that this will be an initialization thread.
 *
 * Currently, this does *not* map the rcv capability into the target component.
 *
 * TODO: In the not-so-distant future, we'll get rid of the rcv
 * type. This API must enable not only the creation of the rcv
 * abstraction, but also enable the corresponding thread and tcap
 * (which will likely persist into the future).
 *
 * The first concrete TODO is to be able to get a pointer to the
 * crt_thd from the structure.
 */
int
crt_rcv_create(struct crt_rcv *r, struct crt_comp *c, thdclosure_index_t closure_id)
{
	struct cos_defcompinfo *defci      = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci         = cos_compinfo_get(defci);
	struct cos_aep_info    *sched_aep  = cos_sched_aep_get(defci);
	struct cos_compinfo    *target_ci  = cos_compinfo_get(c->comp_res);
	tcap_t    tcap;
	thdcap_t  thdcap;
	arcvcap_t rcvcap;

	/* Note that this increases the component's reference count */
	crt_refcnt_take(&c->refcnt);
	assert(target_ci->comp_cap);
	if (closure_id == 0) {
		thdcap = cos_initthd_alloc(cos_compinfo_get(defci), target_ci->comp_cap);
	} else {
		thdcap = cos_thd_alloc_ext(cos_compinfo_get(defci), target_ci->comp_cap, closure_id);
	}
	assert(thdcap);

	/* Allocate the necessary kernel resources */
	assert(c->flags & CRT_COMP_SCHED);
	tcap   = cos_tcap_alloc(ci);
	assert(tcap);
	rcvcap = cos_arcv_alloc(ci, thdcap, tcap, target_ci->comp_cap, sched_aep->rcv);
	assert(rcvcap);

	*r = (struct crt_rcv) {
		.local_aep = (struct cos_aep_info) {
			.tc   = tcap,
			.thd  = thdcap,
			.tid  = 0,
			.rcv  = rcvcap,
			.fn   = NULL,
			.data = NULL
		},
		.c         = c,
		.refcnt    = CRT_REFCNT_INITVAL
	};
	r->aep = &r->local_aep;

	return 0;
}

int
crt_asnd_create(struct crt_asnd *s, struct crt_rcv *r)
{
	struct cos_defcompinfo *defci      = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci         = cos_compinfo_get(defci);
	struct cos_compinfo    *target_ci;
	asndcap_t ascap;

	assert(s && r && r->c && r->c->comp_res);
	assert(r->aep && r->aep->rcv);
	target_ci = cos_compinfo_get(r->c->comp_res);
	assert(target_ci->captbl_cap);
	crt_refcnt_take(&r->refcnt);

	ascap = cos_asnd_alloc(ci, r->aep->rcv, target_ci->captbl_cap);
	assert(ascap);

	*s = (struct crt_asnd) {
		.asnd = ascap,
		.rcv  = r
	};

	return 0;
}

/*
 * Create the initial execution within the given component, either
 * with a thread, or with a scheduling context, depending on if the
 * component is normal or a scheduler.
 */
int
crt_thd_init_create(struct crt_comp *c)
{
	struct cos_defcompinfo *defci      = cos_defcompinfo_curr_get();
	struct cos_compinfo    *target_ci  = cos_compinfo_get(c->comp_res);
	struct cos_aep_info    *target_aep = cos_sched_aep_get(c->comp_res);

	/* Should only be called if initialization is necessary */
	if ((c->flags & CRT_COMP_INITIALIZE) == 0) return -1;
	assert(target_aep->thd == 0); /* should not allow double initialization */
	assert(target_ci->comp_cap);

	crt_refcnt_take(&c->refcnt);
	assert(target_ci->comp_cap);
	target_aep->thd = cos_initthd_alloc(cos_compinfo_get(defci), target_ci->comp_cap);
	assert(target_aep->thd);

	return 0;
}

int
crt_thd_sched_create(struct crt_comp *c)
{
	struct cos_defcompinfo *defci      = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci         = cos_compinfo_get(defci);
	struct cos_compinfo    *target_ci  = cos_compinfo_get(c->comp_res);
	struct cos_aep_info    *target_aep = cos_sched_aep_get(c->comp_res);
	struct crt_rcv r;
	int ret;

	/* Should only be called if initialization is necessary */
	if ((c->flags & CRT_COMP_SCHED) == 0) return -1;
	assert(target_aep->thd == 0); /* should not allow double initialization */
	assert(target_ci->comp_cap);

	if (crt_rcv_create(&r, c, 0)) BUG();

	r.aep = target_aep;
	*target_aep = r.local_aep;
	assert(target_aep->thd && target_aep->tc && target_aep->rcv);

	/* Make the resources accessible in the new scheduler component... */
	ret = cos_cap_cpy_at(target_ci, BOOT_CAPTBL_SELF_INITTHD_CPU_BASE, ci, target_aep->thd);
	assert(ret == 0);
	ret = cos_cap_cpy_at(target_ci, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, ci, target_aep->rcv);
	assert(ret == 0);
	ret = cos_cap_cpy_at(target_ci, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, ci, target_aep->tc);
	assert(ret == 0);

	/*
	 * FIXME:
	 * This is an ugly hack to allow components to do cos_introspect()
	 * - to get thdid
	 * - to get budget on tcap
	 * - other introspect uses
	 *
	 * I don't know a way to get away from this for now!
	 * If it were just thdid, capmgr could have returned the thdids!
	 */
	ret = cos_cap_cpy_at(target_ci, BOOT_CAPTBL_SELF_CT, ci, ci->captbl_cap);
	assert(ret == 0);
	/* FIXME: should subset the permissions for this around time management */
	ret = cos_cap_cpy_at(target_ci, BOOT_CAPTBL_SELF_INITHW_BASE, ci, BOOT_CAPTBL_SELF_INITHW_BASE);
	assert(ret == 0);

	return 0;
}

/*
 * This should be called after crt_thd_sched_create. Capmgrs should be
 * able to manage their own resources.
 */
int
crt_capmgr_create(struct crt_comp *c)
{
	struct cos_defcompinfo *defci      = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci         = cos_compinfo_get(defci);
	struct cos_compinfo    *target_ci  = cos_compinfo_get(c->comp_res);
	int ret;

	assert(c->flags & CRT_COMP_SCHED);

	/* assume CT is already mapped in from sched_create */
	ret = cos_cap_cpy_at(target_ci, BOOT_CAPTBL_SELF_PT, ci, ci->pgtbl_cap);
	assert(ret == 0);
	ret = cos_cap_cpy_at(target_ci, BOOT_CAPTBL_SELF_COMP, ci, ci->comp_cap);
	assert(ret == 0);
	ret = cos_cap_cpy_at(target_ci, BOOT_CAPTBL_SELF_UNTYPED_PT, ci, ci->mi.pgtbl_cap);
	assert(ret == 0);

	return 0;
}
