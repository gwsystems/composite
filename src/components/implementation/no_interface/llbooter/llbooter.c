#include <stdlib.h>
#include <limits.h>

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <cos_types.h>
#include <llprint.h>

#include <elf_loader.h>
#include <initargs.h>
#include <barrier.h>

#include <init.h>

#ifndef BOOTER_MAX_SINV
#define BOOTER_MAX_SINV 256
#endif
#ifndef INITARGS_MAX_PATHNAME
#define INITARGS_MAX_PATHNAME 512
#endif

typedef unsigned long crt_refcnt_t;

#define CRT_REFCNT_INITVAL 1

/*
 * Return 0 on success, non-zero on failure (i.e. calling this on a
 * component that is not already active)
 */
static inline int
crt_refcnt_take(crt_refcnt_t *r)
{
	assert(*r > 0);
	if (ps_faa(r, 1) == 0) BUG();

	return 0;
}

/* return 1 if the component should be freed, 0 otherwise */
static inline int
crt_refcnt_release(crt_refcnt_t *r)
{
	assert(*r > 0);
	if (ps_faa(r, -1) == 0) return 1;

	return 0;
}

static inline int
crt_refcnt_alive(crt_refcnt_t *r)
{
	return *r > 0;
}

typedef enum {
	CRT_COMP_NONE        = 0,
	CRT_COMP_SCHED       = 1, 	/* is this a scheduler? */
	CRT_COMP_DELEG       = 1<<1,	/* does this component require delegating management capabilities to it? */
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
	struct elf_hdr *elf_hdr;
	struct cos_defcompinfo *comp_res;
	struct cos_defcompinfo comp_res_mem;

	crt_refcnt_t refcnt;
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

struct crt_thd {
	thdcap_t cap;
	struct crt_comp *c;
};

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
crt_comp_create(struct crt_comp *c, char *name, compid_t id, struct elf_hdr *elf_hdr, vaddr_t info)
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

	ci            = cos_compinfo_get(c->comp_res);
	root_ci       = cos_compinfo_get(cos_defcompinfo_curr_get());

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
	comp_info = (struct cos_component_information *)(mem + round_up_to_page(ro_sz) + info_offset);
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
	if (crt_refcnt_take(&client->refcnt)) BUG();
	if (crt_refcnt_take(&server->refcnt)) BUG();

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

	if (crt_refcnt_take(&c->refcnt)) BUG();
	assert(target_ci->comp_cap);
	thdcap = cos_thd_alloc_ext(cos_compinfo_get(defci), target_ci->comp_cap, closure_id);
	assert(thdcap);

	*t = (struct crt_thd) {
		.cap = thdcap,
		.c   = c
	};

	return 0;
}

int
crt_rcv_create(struct crt_rcv *r, struct crt_comp *c, thdclosure_index_t closure_id)
{
	struct cos_defcompinfo *defci      = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci         = cos_compinfo_get(defci);
	struct cos_aep_info    *sched_aep  = cos_sched_aep_get(defci);
	struct cos_compinfo    *target_ci  = cos_compinfo_get(c->comp_res);
	struct crt_thd thd;
	tcap_t    tcap;
	thdcap_t  thdcap;
	arcvcap_t rcvcap;

	/* Note that this increase the component's reference count */
	if (crt_thd_create(&thd, c, closure_id)) BUG();
	thdcap = thd.cap;

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
	if (crt_refcnt_take(&r->refcnt)) BUG();

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
static inline int
crt_thd_init_create(struct crt_comp *c)
{
	struct cos_defcompinfo *defci      = cos_defcompinfo_curr_get();
	struct cos_compinfo    *target_ci  = cos_compinfo_get(c->comp_res);
	struct cos_aep_info    *target_aep = cos_sched_aep_get(c->comp_res);

	/* Should only be called if initialization is necessary */
	if ((c->flags & CRT_COMP_INITIALIZE) == 0) return -1;
	assert(target_aep->thd == 0); /* should not allow double initialization */
	assert(target_ci->comp_cap);

	if (crt_refcnt_take(&c->refcnt)) BUG();
	assert(target_ci->comp_cap);
	target_aep->thd = cos_initthd_alloc(cos_compinfo_get(defci), target_ci->comp_cap);
	assert(target_aep->thd);

	return 0;
}

static inline int
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

	return 0;
}


/* Booter's additive information about the component */
typedef enum {
	BOOT_COMP_PREINIT,
	BOOT_COMP_COS_INIT,
	BOOT_COMP_PAR_INIT,
	BOOT_COMP_MAIN, /* type of main is determined by main_type */
	BOOT_COMP_PASSIVE,
	BOOT_COMP_TERM
} boot_comp_state_t;

struct boot_comp {
	volatile boot_comp_state_t state;
	init_main_t main_type; /* does this component have post-initialization execution? */
	struct simple_barrier barrier;
	coreid_t init_core;
	struct crt_comp comp;
};
static struct boot_comp boot_comps[MAX_NUM_COMPS];
static const  compid_t  sched_root_id  = 2;
static        long      boot_id_offset = -1;
/* All synchronous invocations */
static struct crt_sinv  boot_sinvs[BOOTER_MAX_SINV];

/*
 * Assumptions: the component with the lowest id *must* be the one
 * that is passed into this function first. You *can* pass in an id
 * that is higher than we have components. In that case, this will
 * return NULL.
 */
static struct boot_comp *
boot_comp_get(compid_t id)
{
	assert(id > 0);

	if (boot_id_offset == -1) {
		boot_id_offset = id;
	}
	/* casts are OK as we know that boot_id_offset is > 0 now */
	assert((compid_t)boot_id_offset <= id);
	assert(boot_id_offset >= 0 && id >= (compid_t)boot_id_offset);

	return &boot_comps[id - boot_id_offset];
}

static void
boot_comp_set_idoffset(int off)
{
	boot_id_offset = off;
}

static struct boot_comp *
boot_comp_next(compid_t id)
{
	struct boot_comp *c = boot_comp_get(id);
	struct boot_comp *n = boot_comp_get(id + 1);

	assert(c->comp.id == id);
	if (n->comp.id == 0) return NULL;

	return n;
}

static void
comps_init(void)
{
	struct initargs comps, curr;
	struct initargs_iter i;
	int cont, ret, j;
	int comp_idx = 0, sinv_idx = 0;

	/*
	 * Assume: our component id is the lowest of the ids for all
	 * components we are set to create, and that we get it from
	 * mkimg.
	 */
	if (cos_compid_uninitialized()) {
		int booter_id;

		booter_id = atoi(args_get("compid"));
		cos_compid_set(booter_id);
	}
	boot_comp_set_idoffset(cos_compid());

	ret = args_get_entry("components", &comps);
	assert(!ret);
	printc("Components:\n");
	for (cont = args_iter(&comps, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		struct crt_comp *comp;
		struct elf_hdr  *elf;
		struct boot_comp *bc;
		int   keylen;
		compid_t id = atoi(args_key(&curr, &keylen));
		char *name  = args_get_from("img", &curr);
		vaddr_t info = atol(args_get_from("info", &curr));
		const char *root = "binaries/";
		int   len  = strlen(root);
		char  path[INITARGS_MAX_PATHNAME];

		printc("%s: %lu\n", name, id);

		assert(id < MAX_NUM_COMPS && id > 0 && name);

		memset(path, 0, INITARGS_MAX_PATHNAME);
		strncat(path, root, len);
		assert(path[len] == '\0');
		strncat(path, name, INITARGS_MAX_PATHNAME - len);
		assert(path[INITARGS_MAX_PATHNAME - 1] == '\0'); /* no truncation allowed */

		bc  = boot_comp_get(id);
		assert(bc);
		*bc = (struct boot_comp) {
			.state     = BOOT_COMP_PREINIT,
			.main_type = INIT_MAIN_NONE,
			.init_core = cos_cpuid(),
			.barrier   = SIMPLE_BARRIER_INITVAL
		};
		simple_barrier_init(&bc->barrier, init_parallelism());

		comp = &bc->comp;
		elf  = (struct elf_hdr *)args_get(path);

		if (id == cos_compid()) {
			int ret;

			/* booter should not have an elf object */
			assert(!elf);
			ret = crt_booter_create(comp, name, id, info);
			assert(ret == 0);
		} else {
			assert(elf);
			if (crt_comp_create(comp, name, id, elf, info)) {
				printc("Error constructing the resource tables and image of component %s.\n", comp->name);
				BUG();
			}
		}
		assert(cos_compinfo_get(comp->comp_res)->comp_cap);
	}

	ret = args_get_entry("execute", &comps);
	assert(!ret);
	printc("Execution schedule:\n");
	for (cont = args_iter(&comps, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		struct boot_comp    *bc;
		struct crt_comp     *comp;
		int      keylen;
		compid_t id        = atoi(args_key(&curr, &keylen));
		char    *exec_type = args_value(&curr);

		assert(id != cos_compid());
		bc   = boot_comp_get(id);
		assert(bc);
		comp = &bc->comp;

		if (!strcmp(exec_type, "sched")) {
			comp->flags |= CRT_COMP_SCHED;
			if (crt_thd_sched_create(comp)) BUG();
			printc("\tCreated scheduling execution for %ld\n", id);
		} else if (!strcmp(exec_type, "init")) {
			comp->flags |= CRT_COMP_INITIALIZE;
			if (crt_thd_init_create(comp)) BUG();
			printc("\tCreated thread for %ld\n", id);
		} else {
			printc("Error: Found execution schedule type %s.\n", exec_type);
			BUG();
		}

		bc->state = BOOT_COMP_COS_INIT;
	}

	ret = args_get_entry("sinvs", &comps);
	assert(!ret);
	printc("Synchronous invocations (%d):\n", args_len(&comps));
	for (cont = args_iter(&comps, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		struct crt_sinv *sinv;
		int serv_id = atoi(args_get_from("server", &curr));
		int cli_id  = atoi(args_get_from("client", &curr));

		assert(sinv_idx < BOOTER_MAX_SINV);
		sinv = &boot_sinvs[sinv_idx];
		sinv_idx++;	/* bump pointer allocation */

		crt_sinv_create(sinv, args_get_from("name", &curr), &boot_comp_get(serv_id)->comp, &boot_comp_get(cli_id)->comp,
				strtoul(args_get_from("c_fn_addr", &curr), NULL, 10), strtoul(args_get_from("c_ucap_addr", &curr), NULL, 10),
				strtoul(args_get_from("s_fn_addr", &curr), NULL, 10));

		printc("\t%s (%lu->%lu):\tclient_fn @ 0x%lx, client_ucap @ 0x%lx, server_fn @ 0x%lx\n",
		       sinv->name, sinv->client->id, sinv->server->id, sinv->c_fn_addr, sinv->c_ucap_addr, sinv->s_fn_addr);
	}

	printc("Kernel resources created, booting components!\n");

	return;
}

static void
booter_init(void)
{
	struct cos_compinfo *boot_info = cos_compinfo_get(cos_defcompinfo_curr_get());

	cos_meminfo_init(&(boot_info->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();

	cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
}

void
execute(void)
{
	struct boot_comp *boot, *c;
	struct crt_comp *comp;

	assert(cos_compid() > 0);
	boot = boot_comp_get(cos_compid());
	assert(boot);
	comp = &boot->comp;
	assert(comp->id == cos_compid() && crt_is_booter(comp));

	/* Initialize components in order of the pre-computed schedule from mkimg */
	while ((c = boot_comp_next(comp->id))) {
		int initcore = c->init_core == cos_cpuid();
		thdcap_t thd;

		assert(c->state = BOOT_COMP_COS_INIT);
		comp = &c->comp;
		thd  = comp->comp_res->sched_aep[cos_cpuid()].thd;

		if (initcore) {
			assert(thd);
			printc("Initializing component %lu (executing cos_init).\n", comp->id);
		} else {
			/* wait for the init core's thread to initialize */
			while (ps_load(&c->state) == BOOT_COMP_COS_INIT) ;
			if (ps_load(&c->state) != BOOT_COMP_PAR_INIT) continue;

			/* Lazily allocate parallel threads only when they are required. */
			if (!thd) {
				thd = crt_thd_init_create(comp);
				assert(!comp->comp_res->sched_aep[cos_cpuid()].thd);
				comp->comp_res->sched_aep[cos_cpuid()].thd = thd;
				assert(thd);
			}
		}

		if (cos_defswitch(thd, TCAP_PRIO_MAX, TCAP_RES_INF, cos_sched_sync())) BUG();
		assert(c->state > BOOT_COMP_PAR_INIT);
	}

	/*
	 * Initialization of components (parallel or sequential)
	 * complete. Execute the main in components, FIFO
	 */
	comp = &boot->comp;
	while ((c = boot_comp_next(comp->id))) {
		int initcore = c->init_core == cos_cpuid();
		thdcap_t thd;

		comp = &c->comp;
		thd  = comp->comp_res->sched_aep[cos_cpuid()].thd;

		/* wait for the initcore to change the state... */
		while (ps_load(&c->state) == BOOT_COMP_COS_INIT || ps_load(&c->state) == BOOT_COMP_PAR_INIT) ;
		/* If we don't need to continue persistent computation... */
		if (ps_load(&c->state) == BOOT_COMP_PASSIVE ||
		    (c->main_type == INIT_MAIN_SINGLE && !initcore)) continue;

		if (initcore) {
			assert(thd);
			printc("Switching back to main in component %lu.\n", comp->id);
		} else if (!thd) {
			thd = crt_thd_init_create(comp);
			comp->comp_res->sched_aep[cos_cpuid()].thd = thd;
			assert(thd);
		}

		if (cos_defswitch(thd, TCAP_PRIO_MAX, TCAP_RES_INF, cos_sched_sync())) BUG();
	}

	cos_hw_shutdown(BOOT_CAPTBL_SELF_INITHW_BASE);
	while (1) ;

	BUG();

	return;
}

void
init_done(int parallel_init, init_main_t main_type)
{
	compid_t client = (compid_t)cos_inv_token();
	struct boot_comp *c, *n;

	assert(client > 0 && client <= MAX_NUM_COMPS);
	c = boot_comp_get(client);
	assert(c && ps_load(&c->state) > BOOT_COMP_PREINIT);

	switch (ps_load(&c->state)) {
	case BOOT_COMP_COS_INIT: {
		c->main_type = main_type;

		if (parallel_init) {
			/* This will activate any parallel threads */
			ps_store(&c->state, BOOT_COMP_PAR_INIT);
			return; /* we're continuing with initialization, return! */
		}

		if (c->main_type == INIT_MAIN_NONE) ps_store(&c->state, BOOT_COMP_PASSIVE);
		else                                ps_store(&c->state, BOOT_COMP_MAIN);

		break;
	}
	case BOOT_COMP_PAR_INIT: {
		simple_barrier(&c->barrier);
		if (c->init_core != cos_cpuid()) break;

		if (c->main_type == INIT_MAIN_NONE) ps_store(&c->state, BOOT_COMP_PASSIVE);
		else                                ps_store(&c->state, BOOT_COMP_MAIN);

		break;
	}
	default: {
		printc("Error: component %lu past initialization called init_done.\n", c->comp.id);
	}}

	if (c->init_core == cos_cpuid()) {
		printc("Component %lu initialization complete%s.\n", c->comp.id, (c->main_type > 0 ? ", awaiting main execution": ""));
	}

	/* switch back to the booter's thread in execute() */
	if (cos_defswitch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE, TCAP_PRIO_MAX, TCAP_RES_INF, cos_sched_sync())) BUG();

	assert(c->state != BOOT_COMP_PASSIVE);
	assert(c->state != BOOT_COMP_COS_INIT && c->state != BOOT_COMP_PAR_INIT);

	if (c->state == BOOT_COMP_MAIN && c->init_core == cos_cpuid()) {
		printc("Executing main in component %lu.\n", cos_compid());
	}

	return;
}

void
init_exit(int retval)
{
	compid_t client = (compid_t)cos_inv_token();
	struct boot_comp *c;

	assert(client > 0 && client <= MAX_NUM_COMPS);
	c = boot_comp_get(client);

	if (c->init_core == cos_cpuid()) {
		c->state = BOOT_COMP_TERM;
		printc("Component %lu has terminated with error code %d on core %lu.\n",
		       c->comp.id, retval, cos_cpuid());
	}

	/* switch back to the booter's thread in execute */
	if (cos_defswitch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE, TCAP_PRIO_MAX, TCAP_RES_INF, cos_sched_sync())) BUG();
	BUG();
	while (1) ;
}

void
cos_init(void)
{
	booter_init();
	comps_init();
}

void
parallel_main(coreid_t cid)
{
	execute();
}
