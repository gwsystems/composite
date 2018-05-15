/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#ifndef SL_XCPU_H
#define SL_XCPU_H

#include <ck_ring.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <res_spec.h>

#define SL_XCPU_PARAM_MAX 4

typedef enum {
	SL_XCPU_THD_ALLOC = 0,
	SL_XCPU_THD_ALLOC_EXT,
	SL_XCPU_AEP_ALLOC,
	SL_XCPU_AEP_ALLOC_EXT,
	SL_XCPU_INITAEP_ALLOC,
	SL_XCPU_THD_DEALLOC, /* thread delete, need it? */
} sl_xcpu_req_t;

struct sl_xcpu_request {
	sl_xcpu_req_t type;         /* request type */
	cpuid_t       client;       /* client cpu making the request */
	int           req_response; /* client needs a response */
	sched_param_t params[SL_XCPU_PARAM_MAX]; /* scheduling parameters */
	int           param_count;		 /* number of parameters */

	union {
		struct {
			cos_thd_fn_t            fn;
			void                   *data;
		} sl_xcpu_req_thd_alloc;
		struct {
			cos_thd_fn_t            fn;
			void                   *data;
			int                     own_tcap;
			cos_channelkey_t        key;
		} sl_xcpu_req_aep_alloc;
		struct {
			thdclosure_index_t      idx; /* TODO: create thread in another component ? */
			struct cos_defcompinfo *dci;
		} sl_xcpu_req_thd_alloc_ext;
		struct {
			thdclosure_index_t      idx;
			int                     own_tcap;
			cos_channelkey_t        key;
			struct cos_defcompinfo *dci;
		} sl_xcpu_req_aep_alloc_ext;
		struct {
			int                     is_sched;
			int                     own_tcap;
			struct cos_defcompinfo *dci, *sched;
		} sl_xcpu_req_initaep_alloc;
	};
};

CK_RING_PROTOTYPE(xcpu, sl_xcpu_request);

#define SL_XCPU_RING_SIZE (64 * sizeof(struct sl_xcpu_request)) /* in sl_const.h? */

/* perhaps move these to sl.h? */
struct sl_global {
	struct ck_ring xcpu_ring[NUM_CPU]; /* mpsc ring! */

	struct sl_xcpu_request xcpu_rbuf[NUM_CPU][SL_XCPU_RING_SIZE];
	u32_t cpu_bmp[NUM_CPU_BMP_WORDS]; /* bitmap of cpus this scheduler is running on! */
	asndcap_t xcpu_asnd[NUM_CPU][NUM_CPU];
} CACHE_ALIGNED;

extern struct sl_global sl_global_data;

static inline struct sl_global *
sl__globals(void)
{
	return &sl_global_data;
}

static inline int
sl_cpu_active(void)
{
        return bitmap_check(sl__globals()->cpu_bmp, cos_cpuid());
}

static inline struct ck_ring *
sl__ring(cpuid_t cpu)
{
	return &(sl__globals()->xcpu_ring[cpu]);
}

static inline struct ck_ring *
sl__ring_curr(void)
{
	return sl__ring(cos_cpuid());
}

static inline struct sl_xcpu_request *
sl__ring_buffer(cpuid_t cpu)
{
	return (sl__globals()->xcpu_rbuf[cpu]);
}

static inline struct sl_xcpu_request *
sl__ring_buffer_curr(void)
{
	return sl__ring_buffer(cos_cpuid());
}

/* perhaps move these to sl.h? */
int sl_xcpu_thd_alloc(cpuid_t cpu, cos_thd_fn_t fn, void *data, sched_param_t params[]);
int sl_xcpu_thd_alloc_ext(cpuid_t cpu, struct cos_defcompinfo *dci, thdclosure_index_t idx, sched_param_t params[]);
int sl_xcpu_aep_alloc(cpuid_t cpu, cos_thd_fn_t fn, void *data, int own_tcap, cos_channelkey_t key, sched_param_t params[]);
int sl_xcpu_aep_alloc_ext(cpuid_t cpu, struct cos_defcompinfo *dci, thdclosure_index_t idx, int own_tcap, cos_channelkey_t key, sched_param_t params[]);
int sl_xcpu_initaep_alloc(cpuid_t cpu, struct cos_defcompinfo *dci, int own_tcap, cos_channelkey_t key, sched_param_t params[]);
int sl_xcpu_initaep_alloc_ext(cpuid_t cpu, struct cos_defcompinfo *dci, struct cos_defcompinfo *sched, int own_tcap, cos_channelkey_t key, sched_param_t params[]);

#endif /* SL_XCPU_H */
