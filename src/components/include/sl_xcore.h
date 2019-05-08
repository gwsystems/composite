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

	SL_XCORE_THD_PARAM_SET,
	SL_XCORE_THD_WAKEUP,
} sl_xcore_req_t;

struct sl_xcore_request {
	sl_xcore_req_t type;         /* request type */
	cpuid_t        client_core;  /* client cpu making the request */
	thdid_t        client_thd;
	vaddr_t        response;     /* response addr */

	union {
		struct {
			cos_thd_fn_t            fn;
			void                   *data;
			sched_param_t           params[SL_XCORE_PARAM_MAX]; /* scheduling parameters */
			int                     param_count;		 /* number of parameters */
		} sl_xcore_req_thd_alloc;
		struct {
			cos_thd_fn_t            fn;
			void                   *data;
			int                     own_tcap;
			cos_channelkey_t        key;
			sched_param_t           params[SL_XCORE_PARAM_MAX]; /* scheduling parameters */
			int                     param_count;		 /* number of parameters */
		} sl_xcore_req_aep_alloc;
		struct {
			thdclosure_index_t      idx; /* TODO: create thread in another component ? */
			struct cos_defcompinfo *dci;
			sched_param_t           params[SL_XCORE_PARAM_MAX]; /* scheduling parameters */
			int                     param_count;		 /* number of parameters */
		} sl_xcore_req_thd_alloc_ext;
		struct {
			thdclosure_index_t      idx;
			int                     own_tcap;
			cos_channelkey_t        key;
			struct cos_defcompinfo *dci;
			sched_param_t           params[SL_XCORE_PARAM_MAX]; /* scheduling parameters */
			int                     param_count;		 /* number of parameters */
		} sl_xcore_req_aep_alloc_ext;
		struct {
			int                     is_sched;
			int                     own_tcap;
			struct cos_defcompinfo *dci, *sched;
			sched_param_t           params[SL_XCORE_PARAM_MAX]; /* scheduling parameters */
			int                     param_count;		 /* number of parameters */
		} sl_xcore_req_initaep_alloc;
		struct {
			thdid_t tid;
			sched_param_t param;
		} sl_xcore_req_thd_param_set;
		struct {
			thdid_t tid;
		} sl_xcore_req_thd_wakeup;
	};
};

CK_RING_PROTOTYPE(xcore, sl_xcore_request);

#define SL_XCORE_RING_SIZE (64 * sizeof(struct sl_xcore_request)) /* in sl_const.h? */

/* 
 * TODO: unionize with sl_thd? 
 *
 * IMHO, no! This will occupy too much memory if unionized!
 * Plus, that would require that we'd need cpuid in the sl_thd and many
 * branches around in the code for core-local scheduling!
 * Also, making this struct explicit, makes API use explicit.
 * I should only be able to use: param_set(), wakeup() and perhaps free(). 
 *
 * Change my mind! This is a shit ton of wastage with CACHE_ALIGNED!
 */
struct sl_xcore_thd {
	thdid_t thd;
	cpuid_t core;

	asndcap_t asnd[NUM_CPU];
} CACHE_ALIGNED;

struct sl_xcore_thd *sl_xcore_thd_lookup(thdid_t tid, cpuid_t core);
static inline thdid_t
sl_xcore_thd_thdid(struct sl_xcore_thd *t)
{
	return t->thd;
}
static inline cpuid_t
sl_xcore_thd_core(struct sl_xcore_thd *t)
{
	return t->core;
}

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

static inline int
sl_core_active(void)
{
	return bitmap_check(sl__globals()->core_bmp, cos_cpuid());
}

struct sl_xcore_thd *sl_xcore_thd_alloc(cpuid_t core, cos_thd_fn_t fn, void *data, int nparams, sched_param_t params[]);
struct sl_xcore_thd *sl_xcore_thd_alloc_ext(cpuid_t core, struct cos_defcompinfo *dci, thdclosure_index_t idx, int nparams, sched_param_t params[]);
struct sl_xcore_thd *sl_xcore_aep_alloc(cpuid_t core, cos_thd_fn_t fn, void *data, int own_tcap, cos_channelkey_t key, int nparams, sched_param_t params[]);
struct sl_xcore_thd *sl_xcore_aep_alloc_ext(cpuid_t core, struct cos_defcompinfo *dci, thdclosure_index_t idx, int own_tcap, cos_channelkey_t key, int nparams, sched_param_t params[]);
struct sl_xcore_thd *sl_xcore_initaep_alloc(cpuid_t core, struct cos_defcompinfo *dci, int own_tcap, cos_channelkey_t key, int nparams, sched_param_t params[]);
struct sl_xcore_thd *sl_xcore_initaep_alloc_ext(cpuid_t core, struct cos_defcompinfo *dci, struct cos_defcompinfo *sched, int own_tcap, cos_channelkey_t key, int nparams, sched_param_t params[]);
void                 sl_xcore_thd_param_set(struct sl_xcore_thd *t, sched_param_t param);
void                 sl_xcore_thd_wakeup(struct sl_xcore_thd *t);
void                 sl_xcore_thd_wakeup_tid(thdid_t tid, cpuid_t core);

#endif /* SL_XCORE_H */
