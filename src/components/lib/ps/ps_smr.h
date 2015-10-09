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

#ifndef PS_SMR_H
#define PS_SMR_H

#include <ps_global.h>
#include <ps_slab.h>

struct quiescence_timing {
	volatile ps_tsc_t time_in, time_out;
	volatile ps_tsc_t last_known_quiescence;
	char __padding[PS_CACHE_LINE*2 - 3*sizeof(ps_tsc_t)];
} PS_ALIGNED PS_PACKED;

struct other_cpu {
	ps_tsc_t time_in, time_out, time_updated;
};

struct percpu_info {
	/* Quiescence_timing info of this CPU */
	struct quiescence_timing timing;
	/* Quiescence_timing info of other CPUs known by this CPU */
	struct other_cpu timing_others[NUM_CPU];
	/* padding an additional cacheline for prefetching */
	char __padding[CACHE_LINE*2 - (((sizeof(struct other_cpu)*NUM_CPU)+sizeof(struct quiescence_timing)) % CACHE_LINE)];
} PS_ALIGNED PS_PACKED;

struct parsec {
	struct percpu_info timing_info[PS_NUMCORES] PS_ALIGNED;
} PS_ALIGNED;

static inline void
__ps_smr_free(struct parsec *ps, void *buf, struct ps_slab_freelist_clpad *fls, size_t obj_sz, size_t allocsz, int hintern)
{
	struct ps_mheader  *t, *mem = __ps_mhead_get(buf);
	u16_t               coreid  = mem->slab.coreid;
	struct ps_qsc_list *ql;

	assert(ps_coreid() == coreid);
	ql = &fls[coreid].qsc_list;

	t = ql->tail;
	if (likely(t)) t->next  = ql->tail = mem;
	else           ql->head = ql->tail = mem;
}

static inline void *
__ps_smr_alloc(struct parsec *ps, struct ps_mem_percore *fls, size_t obj_sz, u32_t allocsz, int hintern)
{
	struct ps_mem_percore *m  = &fls[ps_coreid()];
	struct ps_qsc_list    *ql = &m->qsc_list;
	struct ps_mheader     *a  = ql->head;

	if (a) {
		ql->head = a->next;
		if (ql->tail == a) ql->tail = NULL;
		__ps_slab_mem_free(&a->data, fls, obj_sz, allocsz, hintern);
	}

	return __ps_slab_mem_alloc(&m->fl, obj_sz, allocsz, hintern, &m->slabheads);
}

static inline void
ps_enter(struct parsec *parsec)
{
	int curr_cpu;
	ps_tsc_t curr_time;
	struct quiescence_timing *timing;

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

	cos_mem_fence();

	return;
}

static inline void
ps_exit(struct parsec *parsec)
{
	int curr_cpu = ps_coreid();
	struct quiescence_timing *timing;

	timing = &(parsec->timing_info[curr_cpu].timing);
	/*
	 * Here we don't require a full memory barrier on x86 -- only
	 * a compiler barrier is enough.
	 */
	cmm_barrier();
	timing->time_out = timing->time_in + 1;

	return;
}

#define PS_PARSLAB_CREATE(name, objsz, allocsz)				\
PS_SLAB_CREATE(name, objsz, allocsz, 0)				        \
inline void *						                \
ps_mem_alloc_##name(struct parsec *ps)					\
{									\
        struct ps_mem_percore *fl = &slab_##name##_freelist;		\
	return __ps_smr_alloc(ps, fl, size, allocsz, 0);		\
}									\
inline void							        \
ps_mem_free_##name(struct parsec *ps, void *buf)			\
{									\
        struct ps_mem_percore *fl = slab_##name##_freelist;		\
	__ps_smr_free(ps, buf, fl, size, allocsz, 0);			\
}

#endif	/* PS_SMR_H */
