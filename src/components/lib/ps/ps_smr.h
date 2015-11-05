/***
 * Copyright 2014-2015 by Gabriel Parmer.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Authors: Qi Wang, interwq@gmail.com, Gabriel Parmer, gparmer@gwu.edu, 2015
 *
 * History:
 * - Started as parsec.c and parsec.h by Qi.
 */

/***
 * A Scalable Memory Reclamation (SMR) technique built off of the slab
 * allocator for parsec (parallel sections).  Maintains a freelist per
 * slab with memory items ordered in terms of the Time Stamp Counter
 * (tsc) taken when the node was freed.  Removal from these queues is
 * governed by quiescence of parallel threads at the time the memory
 * was freed (which might be some time in the past).  This code
 * specifies the policy for when memory flows between the quiescing
 * queues, and the slab memory.  Moving memory back to the slabs is
 * important to enable us to reclaim and migrate memory between cores
 * (each slab is owned by a core), thus there is some balancing to be
 * done here.
 */

#ifndef PS_SMR_H
#define PS_SMR_H

#include <ps_global.h>
#include <ps_slab.h>

#ifndef PS_QLIST_BATCH
#define PS_QLIST_BATCH 128
#endif

struct ps_quiescence_timing {
	volatile ps_tsc_t time_in, time_out;
	volatile ps_tsc_t last_known_quiescence;
	char __padding[PS_CACHE_PAD - 3*sizeof(ps_tsc_t)];
} PS_ALIGNED PS_PACKED;

struct __ps_other_core {
	ps_tsc_t time_in, time_out, time_updated;
};

struct ps_smr_percore {
	/* ps_quiescence_timing info of this CPU */
	struct ps_quiescence_timing timing;
	/* ps_quiescence_timing info of other CPUs known by this CPU */
	struct __ps_other_core timing_others[PS_NUMCORES];
	/* padding an additional cacheline for prefetching */
	char __padding[PS_CACHE_PAD - (((sizeof(struct __ps_other_core)*PS_NUMCORES)+sizeof(struct ps_quiescence_timing)) % PS_CACHE_LINE)];
} PS_ALIGNED PS_PACKED;

struct parsec {
	int refcnt;
	struct ps_smr_percore timing_info[PS_NUMCORES] PS_ALIGNED;
} PS_ALIGNED;

int ps_quiesce_wait(struct parsec *p, ps_tsc_t tsc, ps_tsc_t *qsc);
int ps_try_quiesce (struct parsec *p, ps_tsc_t tsc, ps_tsc_t *qsc);
void ps_init(struct parsec *ps);
struct parsec *ps_alloc(void);
void __ps_smr_reclaim(coreid_t curr, struct ps_qsc_list *ql, struct ps_smr_info *si, struct ps_mem *mem, ps_free_fn_t ffn);


static inline void
__ps_smr_free(void *buf, struct ps_mem *mem, ps_free_fn_t ffn)
{
	struct ps_mheader  *m  = __ps_mhead_get(buf);
	struct ps_smr_info *si;
	struct ps_qsc_list *ql;
	coreid_t curr_core, curr_numa;
	ps_tsc_t tsc;

	/* this is 85% of the cost of the function... */
	tsc = ps_tsc_locality(&curr_core, &curr_numa); 

	si  = &mem->percore[curr_core].smr_info;
	ql  = &si->qsc_list;

	/* 
	 * Note: we currently enqueue remotely freed memory into the
	 * qlist of the core the memory is freed on, later to be moved
	 * to its native core by the remote free logic within the slab
	 * allocator.  This might cause some cache coherency traffic
	 * that we wouldn't otherwise have due to qlist operations
	 * (i.e. writing to the ->next field within the header).
	 */
	__ps_mhead_setfree(m, tsc);
	__ps_qsc_enqueue(ql, m);
	si->qmemcnt++;
	if (unlikely(si->qmemcnt >= si->qmemtarget)) __ps_smr_reclaim(curr_core, ql, si, mem, ffn);
}

static inline void
ps_enter(struct parsec *parsec)
{
	coreid_t curr_cpu, curr_numa;
	ps_tsc_t curr_time;
	struct ps_quiescence_timing *timing;

	curr_time = ps_tsc_locality(&curr_cpu, &curr_numa);

	timing = &(parsec->timing_info[curr_cpu].timing);
	timing->time_in = curr_time;
	/*
	 * The following is needed when we have coarse granularity
	 * time-stamps (i.e. non-cycle granularity, which means we
	 * could have same time-stamp for different events).
	 */
	/* timing->time_out = curr_time - 1; */

	ps_mem_fence();

	return;
}

static inline void
ps_exit(struct parsec *parsec)
{
	int curr_cpu = ps_coreid();
	struct ps_quiescence_timing *timing;

	timing = &(parsec->timing_info[curr_cpu].timing);
	/*
	 * Here we don't require a full memory barrier on x86 -- only
	 * a compiler barrier is enough.
	 */
	ps_cc_barrier();
	timing->time_out = timing->time_in + 1;

	return;
}

#define PS_PARSLAB_CREATE_AFNS(name, objsz, allocsz, allocfn, freefn)			\
PS_SLAB_CREATE_AFNS(name, objsz, allocsz, sizeof(struct ps_slab), allocfn, freefn)	\
static inline void *									\
ps_mem_alloc_##name(void)								\
{ return ps_slab_alloc_##name(); }							\
static inline void									\
__ps_parslab_free_tramp_##name(struct ps_slab *s, size_t sz, coreid_t c)		\
{ (void)sz; ps_slab_free_coreid_##name(s, c); }						\
static inline void									\
ps_mem_free_##name(void *buf)								\
{ __ps_smr_free(buf, &__ps_mem_##name, __ps_parslab_free_tramp_##name); }		\
static void										\
ps_mem_init_##name(struct parsec *ps)							\
{											\
        struct ps_mem_percore *pc = &__ps_mem_##name.percore[0];			\
	int i;										\
	for (i = 0 ; i < PS_NUMCORES ; i++) {						\
		pc[i].smr_info.qmemtarget = PS_QLIST_BATCH;				\
		pc[i].smr_info.qmemcnt    = 0;						\
		pc[i].smr_info.ps         = ps;						\
		ps->refcnt++;								\
	}										\
}											\
static inline int									\
ps_mem_destroy_##name(struct parsec *ps)						\
{											\
        struct ps_mem_percore *pc = &__ps_mem_##name.percore[0];			\
	int i;										\
	for (i = 0 ; i < PS_NUMCORES ; i++) {						\
		if (__ps_qsc_peek(&pc[i].smr_info.qsc_list)        ||			\
		    __ps_qsc_peek(&pc[i].slab_remote.remote_frees) ||			\
		    pc[i].slab_info.fl.list)                       return -1;		\
	}										\
											\
	ps->refcnt--;									\
	return 0;									\
}


#define PS_PARSLAB_CREATE(name, objsz, allocsz)					\
PS_PARSLAB_CREATE_AFNS(name, objsz, allocsz, ps_slab_defalloc, ps_slab_deffree)


#endif	/* PS_SMR_H */
