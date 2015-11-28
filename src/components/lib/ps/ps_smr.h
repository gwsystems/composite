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
void __ps_memptr_init(struct ps_mem *m, struct parsec *ps);
int  __ps_memptr_delete(struct ps_mem *m);


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
	 * (i.e. writing to the ->next field within the header), but
	 * has the large benefit that we don't have to complicate the
	 * free-time ordering of memory chunks in the quiescence list.
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

#define __PS_PARSLAB_CREATE_AFNS(name, objsz, allocsz, headoff, allocfn, freefn)		\
PS_SLAB_CREATE_AFNS(name, objsz, allocsz, headoff, allocfn, freefn)				\
static inline void										\
__ps_parslab_free_tramp_##name(struct ps_mem *m, struct ps_slab *s, size_t sz, coreid_t c)	\
{ (void)sz; ps_slabptr_free_coreid_##name(m, s, c); }						\
static inline void *										\
ps_memptr_alloc_##name(struct ps_mem *m)							\
{ return ps_slabptr_alloc_##name(m); }								\
static inline void *										\
ps_mem_alloc_##name(void)									\
{ return ps_slab_alloc_##name(); }								\
static inline void										\
ps_memptr_free_##name(struct ps_mem *m, void *buf)						\
{ __ps_smr_free(buf, m, __ps_parslab_free_tramp_##name); }					\
static inline void										\
ps_mem_free_##name(void *buf)									\
{ ps_memptr_free_##name(&__ps_mem_##name, buf); }						\
static void											\
ps_memptr_init_##name(struct ps_mem *m, struct parsec *ps)					\
{ __ps_memptr_init(m, ps); }									\
static inline void										\
ps_mem_init_##name(struct parsec *ps)								\
{												\
	ps_slabptr_init_##name(&__ps_mem_##name);						\
	ps_memptr_init_##name(&__ps_mem_##name, ps); 						\
}												\
static inline struct ps_mem *									\
ps_memptr_create_##name(struct parsec *ps)							\
{												\
	struct ps_mem *m = ps_slabptr_create_##name();						\
	if (!m) return NULL;									\
	ps_memptr_init_##name(m, ps);								\
	return m;										\
}												\
static inline int										\
ps_memptr_delete_##name(struct ps_mem *m)							\
{												\
	if (__ps_memptr_delete(m)) return -1;							\
	ps_slabptr_delete_##name(m);								\
	return 0;										\
}												\
static inline int										\
ps_mem_delete_##name(void)									\
{ return ps_memptr_delete_##name(&__ps_mem_##name); }						\

#define PS_PARSLAB_CREATE_AFNS(name, objsz, allocsz, allocfn, freefn)		\
__PS_PARSLAB_CREATE_AFNS(name, objsz, allocsz, sizeof(struct ps_slab), allocfn, freefn)

#define PS_PARSLAB_CREATE(name, objsz, allocsz)					\
PS_PARSLAB_CREATE_AFNS(name, objsz, allocsz, ps_slab_defalloc, ps_slab_deffree)


#endif	/* PS_SMR_H */
