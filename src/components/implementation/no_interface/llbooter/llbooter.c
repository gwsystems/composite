/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2019, The George Washington University
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */

#include <stdlib.h>
#include <limits.h>

#include <initargs.h>
#include <barrier.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <crt.h>
#include <static_alloc.h>

#include <init.h>
#include <addr.h>

#ifndef BOOTER_MAX_SINV
#define BOOTER_MAX_SINV 256
#endif
#ifndef BOOTER_MAX_SCHED
#define BOOTER_MAX_SCHED 1
#endif
#ifndef BOOTER_MAX_INITTHD
#define BOOTER_MAX_INITTHD (MAX_NUM_COMPS - BOOTER_MAX_SCHED)
#endif
#ifndef INITARGS_MAX_PATHNAME
#define INITARGS_MAX_PATHNAME 512
#endif
#ifndef BOOTER_CAPMGR_MB
#define BOOTER_CAPMGR_MB 64
#endif


/* Booter's additive information about the component */
typedef enum {
	BOOT_COMP_PREINIT,
	BOOT_COMP_COS_INIT,
	BOOT_COMP_PAR_INIT,
	BOOT_COMP_MAIN, /* type of main is determined by main_type */
	BOOT_COMP_PASSIVE,
	BOOT_COMP_TERM
} boot_comp_state_t;

typedef enum {
	BOOT_COMP_THD,
	BOOT_COMP_SCHED,
	BOOT_COMP_NO_THD
} boot_comp_exec_t;

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

SA_STATIC_ALLOC(sinv, struct crt_sinv, BOOTER_MAX_SINV);
SA_STATIC_ALLOC(thd,  struct crt_thd,  BOOTER_MAX_INITTHD);
SA_STATIC_ALLOC(rcv,  struct crt_rcv,  BOOTER_MAX_SCHED);

/*
 * Assumptions: the component with the lowest id *must* be the one
 * that is passed into this function first. You *can* pass in an id
 * that is higher than we have components. In that case, this will
 * return NULL.
 */
static struct boot_comp *
boot_comp_get(compid_t id)
{
	assert(id > 0 && id <= MAX_NUM_COMPS);

	if (boot_id_offset == -1) {
		boot_id_offset = id;
	}
	/* casts are OK as we know that boot_id_offset is > 0 now */
	assert((compid_t)boot_id_offset <= id);
	assert(boot_id_offset >= 0 && id >= (compid_t)boot_id_offset);

	return &boot_comps[id - boot_id_offset];
}

static struct boot_comp *
boot_comp_self(void)
{
	return boot_comp_get(cos_compid());
}

static void
boot_comp_set_idoffset(int off)
{
	boot_id_offset = off;
}

static void
comps_init(void)
{
	struct initargs comps, curr;
	struct initargs_iter i;
	int cont, ret, j;
	int comp_idx = 0;

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
	printc("Components (%d):\n", args_len(&comps));
	for (cont = args_iter(&comps, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		struct crt_comp *comp;
		struct boot_comp *bc;
		void *elf_hdr;
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

		comp    = &bc->comp;
		elf_hdr = (void *)args_get(path);

		if (id == cos_compid()) {
			int ret;

			/* booter should not have an elf object */
			assert(!elf_hdr);
			ret = crt_booter_create(comp, name, id, info);
			assert(ret == 0);
		} else {
			assert(elf_hdr);
			if (crt_comp_create(comp, name, id, elf_hdr, info)) {
				printc("Error constructing the resource tables and image of component %s.\n", comp->name);
				BUG();
			}
		}
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
		struct crt_comp_exec_context ctxt = { 0 };

		assert(exec_type);
		assert(id != cos_compid());
		bc   = boot_comp_get(id);
		assert(bc);
		comp = &bc->comp;

		if (!strcmp(exec_type, "sched")) {
			struct crt_rcv *r = sa_rcv_alloc();

			assert(r);
			if (crt_comp_exec(comp, crt_comp_exec_sched_init(&ctxt, r))) BUG();
			printc("\tCreated scheduling execution for %ld\n", id);
		} else if (!strcmp(exec_type, "init")) {
			struct crt_thd *t = sa_thd_alloc();

			assert(t);
			if (crt_comp_exec(comp, crt_comp_exec_thd_init(&ctxt, t))) BUG();
			printc("\tCreated thread for %ld\n", id);
		} else {
			printc("Error: Found unknown execution schedule type %s.\n", exec_type);
			BUG();
		}

		bc->state = BOOT_COMP_COS_INIT;
	}

	/* perform any necessary captbl delegations */
	ret = args_get_entry("captbl_delegations", &comps);
	assert(!ret);
	printc("Capability table delegations (%d capability managers):\n", args_len(&comps));
	/*
	 * for now, assume only one capmgr (allocating untyped memory
	 * gets complex here otherwise)
	 */
	for (cont = args_iter(&comps, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		struct boot_comp *c;
		struct initargs curr_inner;
		struct initargs_iter i_inner;
		int keylen, cont2;
		compid_t capmgr_id;
		struct crt_comp_resources comp_res = { 0 };
		crt_comp_alias_t alias_flags = 0;
		struct boot_comp *target = NULL;

		capmgr_id = atoi(args_key(&curr, &keylen));
		c = boot_comp_get(capmgr_id);
		assert(c);

		printc("\tCapmgr %ld:\n", capmgr_id);
		/* This assumes that all capabilities for a given component are *contiguous* */
		for (cont2 = args_iter(&curr, &i_inner, &curr_inner) ; cont2 ; cont2 = args_iter_next(&i_inner, &curr_inner)) {
			char    *type      = args_get_from("type", &curr_inner);
			capid_t  capno     = atoi(args_key(&curr_inner, &keylen));
			compid_t target_id = atoi(args_get_from("target", &curr_inner));

			/* If we've moved on to the next component, commit the changes for the previous */
			if (target && target != boot_comp_get(target_id)) {
				if (crt_comp_alias_in(&target->comp, &c->comp, &comp_res, alias_flags)) BUG();

				memset(&comp_res, 0, sizeof(struct crt_comp_resources));
				alias_flags = 0;
			}
			target = boot_comp_get(target_id);
			assert(target);

			printc("\t\tCapability #%ld: %s for component %ld\n", capno, type, target_id);
			if (!strcmp(type, "pgtbl")) {
				comp_res.ptc = capno;
				alias_flags |= CRT_COMP_ALIAS_PGTBL;
			} else if (!strcmp(type, "captbl")) {
				comp_res.ctc = capno;
				alias_flags |= CRT_COMP_ALIAS_CAPTBL;
			} else if (!strcmp(type, "comp")) {
				comp_res.compc = capno;
				alias_flags |= CRT_COMP_ALIAS_COMP;
			} else {
				BUG();
			}
		}
		if (crt_comp_alias_in(&target->comp, &c->comp, &comp_res, alias_flags)) BUG();
	}

	/*
	 * *No static capability slot allocations after this point.*
	 *
	 * Past this point, dynamic allocations are made into the
	 * capability tables of the components. Thus any statically
	 * allocated capability ids can be disrupted by these
	 * bump-pointer allocations if they are used past this
	 * point. All of the capability manager static capabilities
	 * should be done already.
	 *
	 * The exception is that capability ids under BOOT_CAPTLB_FREE
	 * are reserved as is, so they can be allocated to regardless.
	 */

	/*
	 * Create the synchronous invocations for the component. This
	 * has to go *after* the updating of the capability frontier
	 * for the hard-coded capabilities as we don't want to use up
	 * those slots for the synchronous invocations.
	 */
	ret = args_get_entry("sinvs", &comps);
	assert(!ret);
	printc("Synchronous invocations (%d):\n", args_len(&comps));
	for (cont = args_iter(&comps, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		struct crt_sinv *sinv;
		int serv_id = atoi(args_get_from("server", &curr));
		int cli_id  = atoi(args_get_from("client", &curr));

		sinv = sa_sinv_alloc();

		crt_sinv_create(sinv, args_get_from("name", &curr), &boot_comp_get(serv_id)->comp, &boot_comp_get(cli_id)->comp,
				strtoul(args_get_from("c_fn_addr", &curr), NULL, 10), strtoul(args_get_from("c_ucap_addr", &curr), NULL, 10),
				strtoul(args_get_from("s_fn_addr", &curr), NULL, 10));

		printc("\t%s (%lu->%lu):\tclient_fn @ 0x%lx, client_ucap @ 0x%lx, server_fn @ 0x%lx\n",
		       sinv->name, sinv->client->id, sinv->server->id, sinv->c_fn_addr, sinv->c_ucap_addr, sinv->s_fn_addr);
	}

	/*
	 * Delegate the untyped memory to the capmgr. This should go
	 * *after* all allocations that use untyped memory, so that we
	 * can delegate away the rest of our memory. FIXME: this might
	 * not be the cause currently, and we rely on a few untyped
	 * regions (after the ones we delegate to the capmgr) for the
	 * parallel thread allocations.
	 */
	ret = args_get_entry("captbl_delegations", &comps);
	assert(!ret);
	for (cont = args_iter(&comps, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		struct boot_comp *c;
		int keylen;
		struct crt_comp_exec_context ctxt = { 0 };

		c = boot_comp_get(atoi(args_key(&curr, &keylen)));
		assert(c);

		/* TODO: generalize. Give the capmgr 64MB for now. */
		if (crt_comp_exec(&c->comp, crt_comp_exec_capmgr_init(&ctxt, BOOTER_CAPMGR_MB * 1024 * 1024))) BUG();
	}

	printc("Kernel resources created, booting components!\n");

	return;
}

unsigned long
addr_get(compid_t id, addr_t type)
{
	compid_t client = (compid_t)cos_inv_token();
	struct boot_comp *c, *target;
	struct cos_compinfo *ci;

	c = boot_comp_get(client);
	/* only capmgrs should be allowed to call this... */
	assert(c && c->comp.flags & CRT_COMP_CAPMGR);

	if (id <= 0 || client > MAX_NUM_COMPS) return 0;
	target = boot_comp_get(id);
	assert(target);

	ci = cos_compinfo_get(target->comp.comp_res);

	switch (type) {
	case ADDR_CAPTBL_FRONTIER:
		return ci->cap_frontier;
	case ADDR_HEAP_FRONTIER:
		return ci->vas_frontier;
	default:
		return 0;
	}
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
	struct initargs comps, curr;
	struct initargs_iter i;
	int cont;
	int ret;

	/* Initialize components in order of the pre-computed schedule from mkimg */
	ret = args_get_entry("execute", &comps);
	assert(!ret);
	for (cont = args_iter(&comps, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		struct boot_comp *c;
		struct crt_comp  *comp;
		int      keylen;
		compid_t id        = atoi(args_key(&curr, &keylen));
		char    *exec_type = args_value(&curr);
		int      initcore;
		thdcap_t thdcap;

		c = boot_comp_get(id);
		assert(c);
		initcore = c->init_core == cos_cpuid();
		assert(c->state = BOOT_COMP_COS_INIT);
		comp     = &c->comp;
		thdcap   = crt_comp_thdcap_get(comp);

		if (initcore) {
			assert(thdcap);
			printc("Initializing component %lu (executing cos_init).\n", comp->id);
		} else {
			/* wait for the init core's thread to initialize */
			while (ps_load(&c->state) == BOOT_COMP_COS_INIT) ;
			if (ps_load(&c->state) != BOOT_COMP_PAR_INIT) continue;

			/* Lazily allocate parallel threads only when they are required. */
			if (!thdcap) {
				assert(0);
				/* ret = crt_thd_init_create(comp); */
				assert(ret == 0);
				thdcap = crt_comp_thdcap_get(comp);
				assert(thdcap);
			}
		}
		assert(thdcap);

		if (cos_defswitch(thdcap, TCAP_PRIO_MAX, TCAP_RES_INF, cos_sched_sync())) BUG();
		assert(c->state > BOOT_COMP_PAR_INIT);
	}

	/*
	 * Initialization of components (parallel or sequential)
	 * complete. Execute the main in components, FIFO
	 */
	/* Initialize components in order of the pre-computed schedule from mkimg */
	ret = args_get_entry("execute", &comps);
	assert(!ret);
	for (cont = args_iter(&comps, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		struct boot_comp *c;
		struct crt_comp  *comp;
		int      keylen;
		compid_t id        = atoi(args_key(&curr, &keylen));
		char    *exec_type = args_value(&curr);
		int initcore;
		thdcap_t thdcap;

		c = boot_comp_get(id);
		assert(c);
		initcore = c->init_core == cos_cpuid();
		comp     = &c->comp;
		thdcap   = crt_comp_thdcap_get(comp);

		/* wait for the initcore to change the state... */
		while (ps_load(&c->state) == BOOT_COMP_COS_INIT || ps_load(&c->state) == BOOT_COMP_PAR_INIT) ;
		/* If we don't need to continue persistent computation... */
		if (ps_load(&c->state) == BOOT_COMP_PASSIVE ||
		    (c->main_type == INIT_MAIN_SINGLE && !initcore)) continue;

		if (initcore) {
			assert(thdcap);
			printc("Switching to main in component %lu.\n", comp->id);
		} else if (!thdcap) {
			assert(0); /* FIXME: Update once the single core is working */
			/* ret = crt_thd_init_create(comp); */
			/* assert(ret == 0); */
			/* thdcap = crt_comp_create_in(comp); */
			/* assert(thdcap); */
		}

		if (cos_defswitch(thdcap, TCAP_PRIO_MAX, TCAP_RES_INF, cos_sched_sync())) BUG();
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
