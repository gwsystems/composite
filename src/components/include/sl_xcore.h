#ifndef SL_XCORE_H
#define SL_XCORE_H

#include <ck_ring.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <res_spec.h>

#define SL_XCORE_PARAM_MAX 4

typedef enum {
	SL_XCORE_THD_ALLOC = 0,
	SL_XCORE_THD_ALLOC_EXT,
	SL_XCORE_AEP_ALLOC,
	SL_XCORE_AEP_ALLOC_EXT,
	SL_XCORE_INITAEP_ALLOC,
	SL_XCORE_THD_DEALLOC, /* thread delete, need it? */
} sl_xcore_req_t;

struct sl_xcore_request {
	sl_xcore_req_t type;         /* request type */
	cpuid_t       client;       /* client cpu making the request */
	int           req_response; /* client needs a response */
	sched_param_t params[SL_XCORE_PARAM_MAX]; /* scheduling parameters */
	int           param_count;		 /* number of parameters */

	union {
		struct {
			cos_thd_fn_t            fn;
			void                   *data;
		} sl_xcore_req_thd_alloc;
		struct {
			cos_thd_fn_t            fn;
			void                   *data;
			int                     own_tcap;
			cos_channelkey_t        key;
		} sl_xcore_req_aep_alloc;
		struct {
			thdclosure_index_t      idx; /* TODO: create thread in another component ? */
			struct cos_defcompinfo *dci;
		} sl_xcore_req_thd_alloc_ext;
		struct {
			thdclosure_index_t      idx;
			int                     own_tcap;
			cos_channelkey_t        key;
			struct cos_defcompinfo *dci;
		} sl_xcore_req_aep_alloc_ext;
		struct {
			int                     is_sched;
			int                     own_tcap;
			struct cos_defcompinfo *dci, *sched;
		} sl_xcore_req_initaep_alloc;
	};
};

CK_RING_PROTOTYPE(xcore, sl_xcore_request);

#define SL_XCORE_RING_SIZE (64 * sizeof(struct sl_xcore_request)) /* in sl_const.h? */

/* perhaps move these to sl.h? */
struct sl_global {
	struct ck_ring xcore_ring[NUM_CPU]; /* mpsc ring! */

	struct sl_xcore_request xcore_rbuf[NUM_CPU][SL_XCORE_RING_SIZE];
	u32_t core_bmp[(NUM_CPU + 7)/8]; /* bitmap of cores this scheduler is running on! */
	asndcap_t xcore_asnd[NUM_CPU][NUM_CPU];
	struct cos_scb_info *scb_area;
} CACHE_ALIGNED;

extern struct sl_global sl_global_data;

static inline struct sl_global *
sl__globals(void)
{
	return &sl_global_data;
}

static inline struct ck_ring *
sl__ring(cpuid_t core)
{
	return &(sl__globals()->xcore_ring[core]);
}

static inline struct ck_ring *
sl__ring_curr(void)
{
	return sl__ring(cos_cpuid());
}

static inline struct sl_xcore_request *
sl__ring_buffer(cpuid_t core)
{
	return (sl__globals()->xcore_rbuf[core]);
}

static inline struct sl_xcore_request *
sl__ring_buffer_curr(void)
{
	return sl__ring_buffer(cos_cpuid());
}

int sl_xcore_thd_alloc(cpuid_t core, cos_thd_fn_t fn, void *data, int nparams, sched_param_t params[]);
int sl_xcore_thd_alloc_ext(cpuid_t core, struct cos_defcompinfo *dci, thdclosure_index_t idx, int nparams, sched_param_t params[]);
int sl_xcore_aep_alloc(cpuid_t core, cos_thd_fn_t fn, void *data, int own_tcap, cos_channelkey_t key, int nparams, sched_param_t params[]);
int sl_xcore_aep_alloc_ext(cpuid_t core, struct cos_defcompinfo *dci, thdclosure_index_t idx, int own_tcap, cos_channelkey_t key, int nparams, sched_param_t params[]);
int sl_xcore_initaep_alloc(cpuid_t core, struct cos_defcompinfo *dci, int own_tcap, cos_channelkey_t key, int nparams, sched_param_t params[]);
int sl_xcore_initaep_alloc_ext(cpuid_t core, struct cos_defcompinfo *dci, struct cos_defcompinfo *sched, int own_tcap, cos_channelkey_t key, int nparams, sched_param_t params[]);

#endif /* SL_XCORE_H */
