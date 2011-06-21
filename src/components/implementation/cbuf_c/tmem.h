#include <cos_component.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_list.h>
//#include <cos_vect.h>

#include <sched.h>
#include <mem_mgr_large.h>

#include <tmem_tk.h>

#define MAX_BLKED 10

#define TAKE() if(sched_component_take(cos_spd_id())) BUG();
#define RELEASE() if(sched_component_release(cos_spd_id())) BUG();

#ifdef _DEBUG_TMEMMGR
#define DOUT(fmt,...) printc(WHERESTR fmt, WHEREARG, ##__VA_ARGS__)
#else
#define DOUT(fmt, ...)
#endif

#define SPD_IS_MANAGED(spd_stk_info) ((spd_stk_info)->managed != 0)

#define SPD_HAS_BLK_THD(spd_stk_info) ((spd_stk_info)->num_blocked_thds != 0)

#define GLOBAL_BLKED (FIRST_LIST(&global_blk_list, next, prev) != &global_blk_list)

/**
 * keep track of thread id's
 * Should this be a typedef'd type?
 */
struct blocked_thd {
	unsigned short int thd_id;
	struct blocked_thd *next, *prev;
};

/**
 * This structure is used to keep
 * track of information and stats about each
 * spd
 */
struct spd_stk_info {
	spdid_t spdid;
	/* The number of stacks in use by spd, and the number want it
	 * to use, and at any point in time the number of threads in
	 * the blocked list. The num_waiting_thds includes the blocked
	 * threads and the threads those are ready but haven't
	 * obtained a stack in this component, which value is useful
	 * for estimating the concurrency of the component.*/
	unsigned int num_allocated, num_desired;
	unsigned int num_blocked_thds,num_waiting_thds;
	/* 0 means not managed by us */
	unsigned int managed;

	unsigned int ss_counter; /* Self-suspension counter */
	/* if ss_counter > 0, at most ss_max items can be over-quota allocated */
	unsigned int ss_max;

	/* Measurements */
	unsigned int nthd_blks[MAX_NUM_THREADS];
	u64_t        thd_blk_start[MAX_NUM_THREADS];
	u64_t        thd_blk_tot[MAX_NUM_THREADS];
	unsigned int stat_thd_blk[MAX_BLKED];

	/* Next two typedef structures are specific data structures
	 * which are defined in tmem_conf.h (in interface dir) */

	/* Shared data-structure between the target component and mgr */
	shared_component_info *ci;

	/* tmem item list */
	tmem_item tmem_list;

	struct blocked_thd bthd_list;
};

// Holds info about stack usage
struct spd_stk_info spd_stk_info_list[MAX_NUM_SPDS];

// Holds all currently free tmem
tmem_item * free_tmem_list;

struct blocked_thd global_blk_list;

/* empty_comps is used to ensure at least one stack per
 * component. over_quota and over_quota_limit save the number of
 * over-quota allocated stacks (due to self-suspension) and the upper
 * limit of it */
int stacks_allocated, stacks_target, empty_comps, over_quota_total, over_quota_limit;

static inline void wake_blk_list(struct blocked_thd * bl);

static inline int
put_mem(tmem_item *tmi)
{
	assert(EMPTY_LIST(tmi, next, prev));
	assert(tmi->parent_spdid == 0);

	stacks_allocated--;
	tmi->free_next = free_tmem_list;
	free_tmem_list = tmi;
	
	if (GLOBAL_BLKED)
		wake_blk_list(&global_blk_list);

	return 0;
}

static inline tmem_item *
get_mem(void)
{
	tmem_item *tmi;

	/* Do we need to maintain global stack target? If we set a
	 * limit to each component, can we get a global target as
	 * well? Disable this first because it prevents
	 * self-suspension stacks over-quota allocation, which is
	 * necessary
	 */
	/* if (stacks_allocated >= stacks_target) return NULL; */
	tmi = free_tmem_list;
	if (!tmi) return NULL;
	free_tmem_list = tmi->free_next;
	stacks_allocated++;

	return tmi;
}

/**
 * gets the number of tmem items associated with a given spdid. Only
 * used in assert since we have item->num_allocated already.
 */
static inline unsigned int
tmem_num_alloc_stks(spdid_t s_spdid)
{
	int count = 0;
	tmem_item *item, *list;

	assert(s_spdid < MAX_NUM_SPDS);
    
	list = &spd_stk_info_list[s_spdid].tmem_list;
	for (item = FIRST_LIST(list, next, prev) ; 
	     item != list ; 
	     item = FIRST_LIST(item, next, prev)) {
		count++;
	}
    
	return count;
}

/* data structure independent function */
static inline void 
tmem_update_stats_block(struct spd_stk_info *ssi, unsigned short int tid)
{
	u64_t start;
	int blked = ssi->num_blocked_thds + 1; /* +1 for us */

	/* printc("************** dude, %d blocked my car in %d (nblocked %d) *****************\n", */
	/*         tid, ssi->spdid, blked); */
	ssi->nthd_blks[tid]++;
	rdtscll(start);
	ssi->thd_blk_start[tid] = start;
	if (MAX_BLKED <= blked) blked = MAX_BLKED-1;
	ssi->stat_thd_blk[blked]++;
}

/* data structure independent function */
static inline void
tmem_update_stats_wakeup(struct spd_stk_info *ssi, unsigned short int tid)
{
	u64_t end, tot;

	/* printc("************** dude, %d found my car in %d *****************\n", */
	/*         tid, ssi->spdid); */
	rdtscll(end);
	tot = end - ssi->thd_blk_start[tid];
	ssi->thd_blk_tot[tid] += tot;
	ssi->thd_blk_start[tid] = 0;
}

static inline void
wake_blk_list(struct blocked_thd *bl)
{
	struct blocked_thd *bthd, *bthd_next;
	spdid_t spdid;
	unsigned int tid;

	spdid = cos_spd_id();
	for(bthd = FIRST_LIST(bl, next, prev) ; bthd != bl ; bthd = bthd_next){
		bthd_next = FIRST_LIST(bthd, next, prev);
		tid = bthd->thd_id;
		DOUT("\t Waking UP thd: %d", tid);
		/* printc("thd %d: ************ waking up thread %d ************\n", */
		/*        cos_get_thd_id(),tid); */
		REM_LIST(bthd, next, prev);
		free(bthd);
		sched_wakeup(spdid, tid);
		DOUT("......UP\n");
	}
}

/* data structure independent function? */
static inline void
tmem_spd_wake_threads(struct spd_stk_info *ssi)
{
	DOUT("waking up threads for spd %d\n", cos_spd_id());
	/* printc("thd %d: ************ start waking up %d threads for spd %d ************\n", */
	/*        cos_get_thd_id(),ssi->num_blocked_thds, spdid); */
	wake_blk_list(&ssi->bthd_list);
	ssi->num_blocked_thds = 0;
	DOUT("All thds now awake\n");

	assert(EMPTY_LIST(&ssi->bthd_list, next, prev));
}

/* data structure independent function */
static inline void
tmem_add_to_blk_list(struct spd_stk_info *ssi, unsigned int tid)
{
	struct blocked_thd *bthd;
	bthd = malloc(sizeof(struct blocked_thd));
	if (unlikely(bthd == NULL)) BUG();

	bthd->thd_id = tid;
	DOUT("Adding thd to the blocked list: %d\n", bthd->thd_id);
	ADD_LIST(&ssi->bthd_list, bthd, next, prev);
	ssi->num_blocked_thds++;
}

static inline void
tmem_add_to_gbl(unsigned int tid)
{
	struct blocked_thd *bthd;
	bthd = malloc(sizeof(struct blocked_thd));
	if (unlikely(bthd == NULL)) BUG();

	bthd->thd_id = tid;
	DOUT("Adding thd to the global blocked list: %d\n", bthd->thd_id);
	ADD_LIST(&global_blk_list, bthd, next, prev);
}

/* return 1 on success */
static inline int
remove_thd_from_blk_list(struct spd_stk_info *ssi, unsigned int tid)
{
	struct blocked_thd *bthd;
	for (bthd = FIRST_LIST(&ssi->bthd_list, next, prev) ;
	     bthd != &ssi->bthd_list ;
	     bthd = FIRST_LIST(bthd, next, prev)) {
		if (bthd->thd_id == tid) {
			REM_LIST(bthd, next, prev);
			free(bthd);
			ssi->num_blocked_thds--;
			return 1;
		}
	}
	/* if not in the local blk list, are we in global blk list? */
	for (bthd = FIRST_LIST(&global_blk_list, next, prev) ;
	     bthd != &global_blk_list ;
	     bthd = FIRST_LIST(bthd, next, prev)) {
		if (bthd->thd_id == tid) {
			REM_LIST(bthd, next, prev);
			free(bthd);
			return 1;
		}
	}
	return 0;
}

/* return 1 if in the local / global block list */
static inline int
tmem_thd_in_blk_list(struct spd_stk_info *ssi, unsigned int tid)
{
	struct blocked_thd *bthd;
	for (bthd = FIRST_LIST(&ssi->bthd_list, next, prev) ;
	     bthd != &ssi->bthd_list ;
	     bthd = FIRST_LIST(bthd, next, prev)) 
		if (bthd->thd_id == tid) return 1;
	/* if not in the local blk list, are we in global blk list? */
	for (bthd = FIRST_LIST(&global_blk_list, next, prev) ;
	     bthd != &global_blk_list ;
	     bthd = FIRST_LIST(bthd, next, prev)) 
		if (bthd->thd_id == tid) return 1;

	return 0;
}

/* data structure independent function */
static inline void
tmem_reset_stats(struct spd_stk_info *ssi)
{
	int i;
	BUG();
	for (i = 0 ; i < MAX_NUM_THREADS ; i++) {
		ssi->nthd_blks[i] = 0;
		ssi->thd_blk_tot[i] = 0;
		ssi->thd_blk_start[i] = 0;
	}
	for (i = 0 ; i < MAX_BLKED ; i++) {
		ssi->stat_thd_blk[i] = 0;
	}
}

static inline int 
tmem_spd_concurrency_estimate(spdid_t spdid)
{
	struct spd_stk_info *ssi;
	unsigned int i, avg;
	unsigned long tot = 0, cnt = 0;
	
	TAKE();
	ssi = get_spd_info(spdid);
	if (!ssi || !SPD_IS_MANAGED(ssi)) {
		goto err;
	}

	if (ssi->num_allocated < ssi->num_desired && !ssi->num_waiting_thds) {
		assert(!SPD_HAS_BLK_THD(ssi));
		avg = ssi->num_allocated;
		goto done;
	}

	for (i = 0 ; i < MAX_BLKED ; i++) {
		int n = ssi->stat_thd_blk[i];

		tot += (n * i);
		cnt += n;
		ssi->stat_thd_blk[i] = 0;
	}

	if (cnt == 0 && ssi->num_waiting_thds == 0) {
		avg = ssi->num_allocated;
	} else {
		unsigned int blk_hist;

		if (cnt) blk_hist = (tot/cnt) + !(tot%cnt == 0); /* adjust for rounding */
		else     blk_hist = 0;

		avg = ssi->num_allocated + (blk_hist > ssi->num_waiting_thds ? 
					    blk_hist : ssi->num_waiting_thds); 
		/* concurrency of self-suspension components can be estimated
		 * more accurately using ss counter */
		if (ssi->ss_counter && avg < (ssi->ss_counter + 1))
			avg = ssi->ss_counter + 1;
	}
done:
	RELEASE();
	return avg;
err:
	RELEASE();
	return -1;
}

static inline void
tmem_report(void)
{
	TAKE();
	/* stkmgr_print_ci_freelist(); */
	printc("allocated: %d,\n", stacks_allocated);
	RELEASE();
}

static inline int
tmem_set_suspension_limit(spdid_t cid, int max)
{
	struct spd_stk_info *ssi;
	TAKE();
	ssi = get_spd_info(cid);
	if (!ssi || !SPD_IS_MANAGED(ssi)) goto err;
	if (max < 0) goto err;

	ssi->ss_max = max;

	RELEASE();
	return 0;
err:
	RELEASE();
	return -1;
}

/* return the self-suspension counter of the component. When reset,
 * keep 75% of the value as history */
static inline int
tmem_detect_suspension(spdid_t cid, int reset)
{
	TAKE();
	struct spd_stk_info * ssi = get_spd_info(cid);
	unsigned int counter = ssi->ss_counter;
	/* +3 for round up. right shift 2 to decrease 25% */
	if (reset) ssi->ss_counter -= (ssi->ss_counter + 3) >> 2;
	RELEASE();
	return counter;
}

static inline int
tmem_set_over_quota_limit(int limit)
{
	TAKE();
	if (limit > MAX_NUM_CBUFS - stacks_target || limit < 0) {
		printc("Over-quota limit greater than global available quota. limit: %d.\n", limit);
		goto err;
	} else
		over_quota_limit = limit;
	RELEASE();
	return 0;
err:
	RELEASE();
	return -1;
}

static inline int
tmem_get_allocated(spdid_t cid)
{
	struct spd_stk_info *ssi;
	int ret;
	TAKE();
	ssi = get_spd_info(cid);
	if (!ssi || !SPD_IS_MANAGED(ssi)) {
		RELEASE();
		return -1;
	}
	ret = ssi->num_allocated;
	RELEASE();
	return ret;
}

static inline unsigned long
tmem_get_thd_blk_time(unsigned short int tid, spdid_t spdid, int reset)
{
	struct spd_stk_info *ssi;
	unsigned long ret;
	long long a = 0;
	u64_t t;

	TAKE();
	ssi = get_spd_info(spdid);
	if (!ssi || !SPD_IS_MANAGED(ssi) || tid >= MAX_NUM_THREADS) {
		goto err;
	}

	/* currently blocked? */
	if (ssi->thd_blk_start[tid]) {
		rdtscll(t);
		a += t - ssi->thd_blk_start[tid];
	}
	if (ssi->nthd_blks[tid]) {
		a = (a + ssi->thd_blk_tot[tid])/ssi->nthd_blks[tid];
	} 
	if (reset) {
		ssi->thd_blk_tot[tid] = 0;
		ssi->nthd_blks[tid] = 0;
	}
	ret = (a >> 20) + ! ((a & 1048575) == 0);/* right shift 20 bits and round up, 2^20 - 1 = 1048575 */
	RELEASE();

	return ret;
err:
	RELEASE();
	return -1;
}

static inline int
tmem_get_thd_blk_cnt(unsigned short int tid, spdid_t spdid, int reset)
{
	struct spd_stk_info *ssi;
	int n;

	TAKE();
	ssi = get_spd_info(spdid);
	if (!ssi || !SPD_IS_MANAGED(ssi) || tid >= MAX_NUM_THREADS) {
		goto err;
	}

	n = ssi->nthd_blks[tid];
	/* Thread on the blocked list? */
	if (ssi->thd_blk_start[tid] && n == 0) n = 1;
	if (reset) {
		ssi->thd_blk_tot[tid] = 0;
		ssi->nthd_blks[tid] = 0;
	}

	RELEASE();
	return n;
err:
	RELEASE();
	return -1;
}

static inline void
tmem_spd_meas_reset(void)
{
	struct spd_stk_info *ssi;
	int i;

	TAKE();
	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		ssi = get_spd_info(i);
		if (!ssi) BUG();
		if (!SPD_IS_MANAGED(ssi)) continue;
		
		tmem_reset_stats(ssi);
	}
	RELEASE();
}
