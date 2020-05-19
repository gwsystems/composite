/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <cos_kernel_api.h>
#include <ps.h>
#include <sched_info.h>
#include <sl.h>
#include <initargs.h>

#define SCHED_MAX_CHILD_COMPS 8
static struct sched_childinfo childinfo[NUM_CPU][SCHED_MAX_CHILD_COMPS];
static unsigned int sched_num_child[NUM_CPU] CACHE_ALIGNED;
static unsigned int sched_num_childsched[NUM_CPU] CACHE_ALIGNED;

unsigned int self_init[NUM_CPU] CACHE_ALIGNED, num_child_init[NUM_CPU] CACHE_ALIGNED;

/* implementation specific initialization per child */
extern void sched_child_init(struct sched_childinfo *schedci);

struct sched_childinfo *
sched_childinfo_find(spdid_t id)
{
	unsigned int i;

	for (i = 0; i < sched_num_child[cos_cpuid()]; i ++) {
		if (childinfo[cos_cpuid()][i].id == id) return &(childinfo[cos_cpuid()][i]);
	}

	return NULL;
}

struct sched_childinfo *
sched_childinfo_alloc(spdid_t id, compcap_t compcap, comp_flag_t flags)
{
	int idx = 0;
	struct sched_childinfo *sci = NULL;
	struct cos_defcompinfo *dci = NULL;

	assert(sched_num_child[cos_cpuid()] < SCHED_MAX_CHILD_COMPS - 1);
	idx = ps_faa((unsigned long *)&sched_num_child[cos_cpuid()], 1);
	sci = &childinfo[cos_cpuid()][idx];
	dci = sched_child_defci_get(sci);

	if (compcap) {
		struct cos_compinfo *ci = cos_compinfo_get(dci);

		ci->comp_cap = compcap;
	} else {
		cos_defcompinfo_childid_init(dci, id);
	}
	sci->id = id;
	sci->flags = flags;

	return sci;
}

unsigned int
sched_num_child_get(void)
{
	return sched_num_child[cos_cpuid()];
}

unsigned int
sched_num_childsched_get(void)
{
	return sched_num_childsched[cos_cpuid()];
}

static void
sched_childinfo_init_intern(int is_raw)
{
	spdid_t child;
	comp_flag_t childflags;

	struct initargs cap_entries, exec_entries, curr;
	struct initargs_iter i;
	vaddr_t vasfr = 0;
	capid_t capfr = 0;
	int ret, cont;

	int remaining = 0;
	int num_comps = 0;

	assert(!is_raw);

	memset(childinfo[cos_cpuid()], 0, sizeof(struct sched_childinfo) * SCHED_MAX_CHILD_COMPS);

	/* Create execution in the relevant components */
	ret = args_get_entry("execute", &exec_entries);
	assert(!ret);
	printc("Sched: %d components that need execution\n", args_len(&exec_entries));
	for (cont = args_iter(&exec_entries, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		int      keylen;
		compid_t id        = atoi(args_key(&curr, &keylen));
		char    *exec_type = args_value(&curr);
		struct cos_defcompinfo *child_dci = NULL;
		struct sched_childinfo *schedinfo = NULL;
		struct sl_thd          *initthd   = NULL;
		compcap_t               compcap   = 0;
		int cpu;

		assert(exec_type);
		assert(id != cos_compid());

		/* Only init threads allowed */
		if (strcmp(exec_type, "init")) {
			BUG();	/* TODO: no support for hierarchical scheduling yet */
		}
		child = id;
		childflags = 0;
		schedinfo = sched_childinfo_alloc(child, compcap, childflags);
		assert(schedinfo);
		child_dci = sched_child_defci_get(schedinfo);
		assert(child_dci && child_dci->id != 0);
		for (cpu = 0; cpu < NUM_CPU; cpu++) {
			bitmap_set(schedinfo->cpubmp, cpu);
		}

		if (bitmap_check(schedinfo->cpubmp, cos_cpuid())) {
			PRINTLOG(PRINT_DEBUG, "Initializing child component %u, is_sched=%d\n", child, childflags & COMP_FLAG_SCHED);
			initthd = sl_thd_initaep_alloc(child_dci, NULL, childflags & COMP_FLAG_SCHED, childflags & COMP_FLAG_SCHED ? 1 : 0, 0, 0, 0); /* TODO: rate information */
			assert(initthd);
			sched_child_initthd_set(schedinfo, initthd);

			sched_child_init(schedinfo);
			if (childflags & COMP_FLAG_SCHED) ps_faa((unsigned long *)&sched_num_childsched[cos_cpuid()], 1);
		}
	}

	assert(sched_num_child_get()); /* at least 1 child component */
}

void
sched_childinfo_init(void)
{
	sched_childinfo_init_intern(0);
}

void
sched_childinfo_init_raw(void)
{
	sched_childinfo_init_intern(1);
}
