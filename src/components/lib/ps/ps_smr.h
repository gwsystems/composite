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

#define QLIST_BATCH_REMOVE 32

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
	struct ps_smr_percore timing_info[PS_NUMCORES] PS_ALIGNED;
} PS_ALIGNED;

int ps_quiesce_wait(struct parsec *p, ps_tsc_t tsc);
int ps_try_quiesce (struct parsec *p, ps_tsc_t tsc);
void ps_init(struct parsec *ps);
struct parsec *ps_alloc(void);

static inline void
__ps_smr_free(struct parsec *ps, void *buf, struct ps_mem_percore *fls, size_t obj_sz, size_t allocsz, int hintern, size_t qthresh)
{
	struct ps_mheader     *mem = __ps_mhead_get(buf);
	u16_t               coreid = mem->slab->coreid;
	struct ps_mem_percore *fl  = &fls[coreid];
	struct ps_qsc_list *ql;
	(void)ps; (void)obj_sz; (void)allocsz; (void)hintern;

	if (unlikely(ps_coreid() != coreid)) {
		__ps_slab_mem_free(buf, fls, obj_sz, allocsz, hintern);
		return;
	}
	
	ql = &fl->qsc_list;
	__ps_qsc_enqueue(ql, mem);
	fl->qmemcnt++;

	if (fl->qmemcnt >= qthresh) {
		struct ps_mheader *a  = __ps_qsc_dequeue(ql);
		/* .... */
		if (a) {
			m->qmemcnt--;
			__ps_slab_mem_free(&a[1], fls, obj_sz, allocsz, hintern);
		}
	}
}

static inline void *
__ps_smr_alloc(struct parsec *ps, struct ps_mem_percore *fls, size_t obj_sz, u32_t allocsz, int hintern)
{
	struct ps_mem_percore *m  = &fls[ps_coreid()];
	struct ps_qsc_list    *ql = &m->qsc_list;
	(void)ps;

	return __ps_slab_mem_alloc(&m->fl, obj_sz, allocsz, hintern, &m->slabheads);
}

static inline void
ps_enter(struct parsec *parsec)
{
	int curr_cpu;
	ps_tsc_t curr_time;
	struct ps_quiescence_timing *timing;

	curr_cpu  = ps_coreid();
	curr_time = ps_tsc();

	timing = &(parsec->timing_info[curr_cpu].timing);
	timing->time_in = curr_time;
	/*
	 * The following is needed when we have coarse granularity
	 * time-stamps (i.e. non-cycle granularity, which means we
	 * could have same time-stamp for different events).
	 */
	timing->time_out = curr_time - 1;

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

#define PS_PARSLAB_CREATE(name, objsz, allocsz, qthresh)		\
PS_SLAB_CREATE(name, objsz, allocsz, 1)				        \
inline void *						                \
ps_mem_alloc_##name(void)						\
{									\
        struct ps_mem_percore *fl = slab_##name##_freelist;		\
	assert(fl->ps);						        \
	return __ps_smr_alloc(fl->ps, fl, objsz, allocsz, 1);		\
}									\
inline void							        \
ps_mem_free_##name(void *buf)						\
{									\
        struct ps_mem_percore *fl = slab_##name##_freelist;		\
	assert(fl->ps);						        \
	__ps_smr_free(fl->ps, buf, fl, objsz, allocsz, 1, qthresh);	\
}									\
void									\
ps_mem_init_##name(struct parsec *ps)					\
{									\
        struct ps_mem_percore *fl = slab_##name##_freelist;		\
	fl->ps = ps;						        \
}

#endif	/* PS_SMR_H */
