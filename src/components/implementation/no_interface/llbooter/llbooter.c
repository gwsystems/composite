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

#include <init.h>
#include <addr.h>

#ifndef BOOTER_MAX_SINV
#define BOOTER_MAX_SINV 256
#endif
#ifndef INITARGS_MAX_PATHNAME
#define INITARGS_MAX_PATHNAME 512
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
	assert(id > 0 && id <= MAX_NUM_COMPS);

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

static void
comps_init(void)
{
	struct initargs comps, curr;
	struct initargs_iter i;
	int cont, ret, j;
	int comp_idx = 0, sinv_idx = 0;
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());

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

		assert(exec_type);
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
			printc("Error: Found unknown execution schedule type %s.\n", exec_type);
			BUG();
		}

		bc->state = BOOT_COMP_COS_INIT;
	}

	/* perform any captbl delegations that are necessary */
	ret = args_get_entry("captbl_delegations", &comps);
	assert(!ret);
	printc("Capability table delegations (%d capability managers):\n", args_len(&comps));
	for (cont = args_iter(&comps, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		struct boot_comp *c;
		struct initargs curr_inner;
		struct initargs_iter i_inner;
		int keylen, cont2;
		compid_t capmgr_id;
		capid_t frontier = BOOT_CAPTBL_FREE;

		capmgr_id = atoi(args_key(&curr, &keylen));
		c = boot_comp_get(capmgr_id);
		assert(c);

		c->comp.flags |= CRT_COMP_CAPMGR;
		crt_capmgr_create(&c->comp);

		printc("\tCapmgr %ld:\n", capmgr_id);

		for (cont2 = args_iter(&curr, &i_inner, &curr_inner) ; cont2 ; cont2 = args_iter_next(&i_inner, &curr_inner)) {
			char *target  = args_get_from("target", &curr_inner);
			char *type    = args_get_from("type", &curr_inner);
			capid_t capno = atoi(args_key(&curr_inner, &keylen));
			struct cos_compinfo *cm_ci  = cos_compinfo_get(c->comp.comp_res);

			printc("\t\tCapability #%ld: %s for component %s\n", capno, type, target);
			if (!strcmp(type, "pgtbl")) {
				ret = cos_cap_cpy_at(cm_ci, capno, ci, ci->pgtbl_cap);
				assert(ret == 0);
			} else if (!strcmp(type, "captbl")) {
				ret = cos_cap_cpy_at(cm_ci, capno, ci, ci->captbl_cap);
				assert(ret == 0);
			} else if (!strcmp(type, "comp")) {
				ret = cos_cap_cpy_at(cm_ci, capno, ci, ci->comp_cap);
				assert(ret == 0);
			} else {
				BUG();
			}
			if (frontier < capno) frontier = capno;
		}
		crt_captbl_frontier_update(&c->comp, round_up_to_pow2(frontier + 1, 4));
	}


	/* Create the synchronous invocations for the component */
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

		printc("\t%s (%u->%u)\n", args_get_from("name", &curr), cli_id, serv_id);

		crt_sinv_create(sinv, args_get_from("name", &curr), &boot_comp_get(serv_id)->comp, &boot_comp_get(cli_id)->comp,
				strtoul(args_get_from("c_fn_addr", &curr), NULL, 10), strtoul(args_get_from("c_ucap_addr", &curr), NULL, 10),
				strtoul(args_get_from("s_fn_addr", &curr), NULL, 10));

		printc("\t%s (%lu->%lu):\tclient_fn @ 0x%lx, client_ucap @ 0x%lx, server_fn @ 0x%lx\n",
		       sinv->name, sinv->client->id, sinv->server->id, sinv->c_fn_addr, sinv->c_ucap_addr, sinv->s_fn_addr);
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
		int initcore;
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
				ret = crt_thd_init_create(comp);
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
			ret = crt_thd_init_create(comp);
			assert(ret == 0);
			thdcap = crt_comp_thdcap_get(comp);
			assert(thdcap);
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
