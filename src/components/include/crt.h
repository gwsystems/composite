#ifndef CRT_H
#define CRT_H

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <cos_types.h>

typedef unsigned long crt_refcnt_t;

typedef enum {
	CRT_COMP_NONE        = 0,
	CRT_COMP_SCHED       = 1, 	/* is this a scheduler? */
	CRT_COMP_CAPMGR      = 1<<1,	/* does this component require delegating management capabilities to it? */
	CRT_COMP_SCHED_DELEG = 1<<2,	/* is the system thread initialization delegated to this component? */
	CRT_COMP_DERIVED     = 1<<4, 	/* derived/forked from another component */
	CRT_COMP_INITIALIZE  = 1<<8,	/* The current component should initialize this component... */
	CRT_COMP_BOOTER      = 1<<16,	/* Is this the current component (i.e. the booter)? */
} crt_comp_flags_t;

struct crt_comp {
	crt_comp_flags_t flags;
	char *name;
	compid_t id;
	vaddr_t entry_addr, ro_addr, rw_addr, info;

	char *mem;		/* image memory */
	pgtblcap_t capmgr_untyped_mem;
	struct elf_hdr *elf_hdr;
	struct cos_defcompinfo *comp_res;
	struct cos_defcompinfo comp_res_mem;

	crt_refcnt_t refcnt;
};

struct crt_thd {
	thdcap_t cap;
	struct crt_comp *c;
};

struct crt_rcv {
	/* The component the aep is attached to */
	struct crt_comp *c;
	/* Local information in this component */
	struct cos_aep_info *aep; /* either points to local_aep, or a component's aep */
	struct cos_aep_info local_aep;

	crt_refcnt_t refcnt; 	/* senders create references */
};

struct crt_asnd {
	struct crt_rcv *rcv;
	asndcap_t asnd;
};

struct crt_sinv {
	char *name;
	struct crt_comp *server, *client;
	vaddr_t c_fn_addr, c_ucap_addr;
	vaddr_t s_fn_addr;
	sinvcap_t sinv_cap;
};

static inline thdcap_t
crt_comp_thdcap_get(struct crt_comp *c)
{
	assert(c && c->comp_res);

	return c->comp_res->sched_aep[cos_cpuid()].thd;
}

static inline void
crt_comp_thdcap_set(struct crt_comp *c, thdcap_t t)
{
	assert(c && c->comp_res);

	c->comp_res->sched_aep[cos_cpuid()].thd = t;
}

int crt_comp_create(struct crt_comp *c, char *name, compid_t id, void *elf_hdr, vaddr_t info);
void crt_captbl_frontier_update(struct crt_comp *c, capid_t capid);
int crt_booter_create(struct crt_comp *c, char *name, compid_t id, vaddr_t info);
int crt_sinv_create(struct crt_sinv *sinv, char *name, struct crt_comp *server, struct crt_comp *client, vaddr_t c_fn_addr, vaddr_t c_ucap_addr, vaddr_t s_fn_addr);
int crt_asnd_create(struct crt_asnd *s, struct crt_rcv *r);

int crt_rcv_create(struct crt_rcv *r, struct crt_comp *c, thdclosure_index_t closure_id);
/*
 * TODO:
 * struct crt_tcap {
 * 	// ...
 * };
 * struct crt_thd *crt_rcv_thd(struct crt_rcv *r);
 * struct crt_tcap *crt_rcv_tcap(struct crt_rcv *r);
 *
 */
int crt_thd_create(struct crt_thd *t, struct crt_comp *c, thdclosure_index_t closure_id);
int crt_thd_init_create(struct crt_comp *c);

int crt_thd_sched_create(struct crt_comp *c);
int crt_capmgr_create(struct crt_comp *c, unsigned long memsz);

/*
 * TODO:
 * struct crt_tcap {
 * 	// ...
 * };
 * struct crt_thd *crt_rcv_thd(struct crt_rcv *r);
 * struct crt_tcap *crt_rcv_tcap(struct crt_rcv *r);
 *
 * Remove the specialized thread creation functions, and replace with
 * flags passed to component, thread, and rcv creation.
 */

#endif /* CRT_H */
