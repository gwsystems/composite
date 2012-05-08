#ifndef TMEM_H
#define TMEM_H

#include <cos_component.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_list.h>
#include <cos_debug.h>

#include <sched.h>
#include <mem_mgr_large.h>

#include <tmem_tk.h>
#include <mem_pool.h>

/* #include <cos_synchronization.h> */
/* cos_lock_t tmem_l; */
/* #define TAKE()  do { if (lock_take(&tmem_l) != 0) BUG(); } while(0) */
/* #define RELEASE() do { if (lock_release(&tmem_l) != 0) BUG() } while(0) */
/* #define LOCK_INIT()    lock_static_init(&tmem_l); */

#define MAX_BLKED 10

#define SPD_IS_MANAGED(spd_tmem_info) ((spd_tmem_info)->managed != 0)

#define SPD_HAS_BLK_THD(spd_tmem_info) ((spd_tmem_info)->num_blocked_thds != 0)

#define SPD_HAS_BLK_THD_ON_GLB(spd_tmem_info) ((spd_tmem_info)->num_glb_blocked != 0)

#define GLOBAL_BLKED (FIRST_LIST(&global_blk_list, next, prev) != &global_blk_list)

/**
 * keep track of thread id's
 * Should this be a typedef'd type?
 */
struct blocked_thd {
	unsigned short int thd_id;
	spdid_t spdid;
	struct blocked_thd *next, *prev;
};

/**
 * This structure is used to keep
 * track of information and stats about each
 * spd
 */
struct spd_tmem_info {
	spdid_t spdid;
	/* The number of tmems in use by spd, and the number want it
	 * to use, and at any point in time the number of threads in
	 * the blocked list. The num_waiting_thds includes the blocked
	 * threads and the threads those are ready but haven't
	 * obtained a tmem in this component, which value is useful
	 * for estimating the concurrency of the component.*/
	unsigned int num_allocated, num_desired;
	unsigned int num_blocked_thds,num_waiting_thds;
	unsigned int num_glb_blocked;
	/* 0 means not managed by us */
	unsigned int managed;
	unsigned int relinquish_mark;

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
	shared_component_info ci;

	/* tmem item list */
	tmem_item tmem_list;

	struct blocked_thd bthd_list;
};

// Holds info about mem usage
struct spd_tmem_info spd_tmem_info_list[MAX_NUM_SPDS];

// Holds all currently free tmem
tmem_item * free_tmem_list;

struct blocked_thd global_blk_list;

/* empty_comps is used to ensure at least one stack per
 * component. over_quota and over_quota_limit save the number of
 * over-quota allocated stacks (due to self-suspension) and the upper
 * limit of it */
int tmems_allocated, tmems_target, empty_comps, over_quota_total, over_quota_limit;

static inline void wake_glb_blk_list(spdid_t spdid, int only_wake_first);
static inline void wake_local_blk_list(struct spd_tmem_info *sti, int only_wake_first);

/**
 * gets the number of tmem items associated with a given spdid. Only
 * used in assert since we have item->num_allocated already.
 */
static inline unsigned int
tmem_num_alloc_tmems(spdid_t s_spdid)
{
	int count = 0;
	tmem_item *item, *list;

	assert(s_spdid < MAX_NUM_SPDS);
    
	list = &spd_tmem_info_list[s_spdid].tmem_list;
	for (item = FIRST_LIST(list, next, prev) ; 
	     item != list ; 
	     item = FIRST_LIST(item, next, prev)) {
		count++;
	}
    
	return count;
}

/* data structure independent function */
static inline void 
tmem_update_stats_block(struct spd_tmem_info *sti, unsigned short int tid)
{
	u64_t start;
	int blked = sti->num_blocked_thds + 1; /* +1 for us */

	DOUT("************** dude, %d blocked my car in %d (nblocked %d) *****************\n",
	        tid, sti->spdid, blked);
	sti->nthd_blks[tid]++;
	rdtscll(start);
	sti->thd_blk_start[tid] = start;
	if (MAX_BLKED <= blked) blked = MAX_BLKED-1;
	sti->stat_thd_blk[blked]++;
}

/* data structure independent function */
static inline void
tmem_update_stats_wakeup(struct spd_tmem_info *sti, unsigned short int tid)
{
	u64_t end, tot;

	DOUT("************** dude, %d found my car in %d *****************\n",
	        tid, sti->spdid);
	rdtscll(end);
	tot = end - sti->thd_blk_start[tid];
	sti->thd_blk_tot[tid] += tot;
	sti->thd_blk_start[tid] = 0;
}


static inline struct blocked_thd *
__wake_glb_thread(struct blocked_thd *bthd)
{
	struct spd_tmem_info *sti;
	struct blocked_thd *bthd_next;
	unsigned int tid;

	bthd_next = FIRST_LIST(bthd, next, prev);
	sti = get_spd_info(bthd->spdid);
	sti->num_glb_blocked--;
	tid = bthd->thd_id;
	DOUT("thd %d: ************ waking up thread %d ************\n",
	       cos_get_thd_id(),tid);
	REM_LIST(bthd, next, prev);
	free(bthd);
	sched_wakeup(cos_spd_id(), tid);
	DOUT("......UP\n");

	return bthd_next;
}

/* if spdid is 0, means wake all thds in glb. */
static inline void
wake_glb_blk_list(spdid_t spdid, int only_wake_first)
{
	int woken = 0;
	struct blocked_thd *bthd, *bthd_next;

	DOUT("thd %d now wake glb, spd %d!\n",cos_get_thd_id(),spdid);

	if (!spdid){  // all thds on glb blk list
		for(bthd = FIRST_LIST(&global_blk_list, next, prev) ; bthd != &global_blk_list ; bthd = bthd_next) {
			bthd_next = __wake_glb_thread(bthd);
			woken++;
			if (only_wake_first) break;
		}
	} else {
		for(bthd = FIRST_LIST(&global_blk_list, next, prev) ; bthd != &global_blk_list ; bthd = bthd_next) {
			if (spdid == bthd->spdid){   // only thds in that spd on glb blk list
				DOUT("wake %d in spd %d!\n",bthd->thd_id, bthd->spdid);
				bthd_next = __wake_glb_thread(bthd);
				woken++;
			} else {
				bthd_next = FIRST_LIST(bthd, next, prev);
			}
			if (only_wake_first) break;
		}
	}

	if (woken && EMPTY_LIST(&global_blk_list, next, prev))
		mempool_clear_glb_blked(cos_spd_id());
	DOUT("done wake up glb!\n");
}

/* wake up all blked threads on local list */
static inline void
wake_local_blk_list(struct spd_tmem_info *sti, int only_wake_first)
{
	struct blocked_thd *bl, *bthd, *bthd_next;
	spdid_t spdid;
	unsigned int tid;

	spdid = sti->spdid;
	bl = &sti->bthd_list;

	for (bthd = FIRST_LIST(bl, next, prev) ; 
	     bthd != bl ; 
	     bthd = bthd_next) {
		bthd_next = FIRST_LIST(bthd, next, prev);
		tid = bthd->thd_id;
		DOUT("thd %d: ************ waking up thread %d ************\n",
		       cos_get_thd_id(),tid);
		REM_LIST(bthd, next, prev);
		free(bthd);
		sched_wakeup(spdid, tid);
		DOUT("......UP\n");
		assert(sti->num_blocked_thds);
		sti->num_blocked_thds--;
		if (only_wake_first) break;
	}
}

/* Wake up threads on local blk list for this spd */
static inline void
tmem_spd_wake_threads(struct spd_tmem_info *sti)
{
	DOUT("waking up local threads for spd %ld\n", cos_spd_id());
	DOUT("thd %d: ************ start waking up %d threads for spd %d ************\n",
	       cos_get_thd_id(),sti->num_blocked_thds, sti->spdid);

	wake_local_blk_list(sti, 0);
	assert(EMPTY_LIST(&sti->bthd_list, next, prev));
	DOUT("All thds now awake\n");

        /* Only wake up threads on global blk list that associates this spd*/
	if (SPD_HAS_BLK_THD_ON_GLB(sti))
		wake_glb_blk_list(sti->spdid, 0);
}

/* Wake up the first thread on local blk list for this spd */
static inline void
tmem_spd_wake_first_thread(struct spd_tmem_info *sti)
{
	DOUT("waking up the first local threads for spd %ld\n", cos_spd_id());
	wake_local_blk_list(sti, 1);

	if (SPD_HAS_BLK_THD_ON_GLB(sti))
		wake_glb_blk_list(sti->spdid, 1);
}

/* data structure independent function */
static inline void
tmem_add_to_blk_list(struct spd_tmem_info *sti, unsigned int tid)
{
	struct blocked_thd *bthd;
	bthd = malloc(sizeof(struct blocked_thd));
	if (unlikely(bthd == NULL)) BUG();

	bthd->thd_id = tid;
	DOUT("Adding thd to the blocked list: %d\n", bthd->thd_id);
	ADD_LIST(&sti->bthd_list, bthd, next, prev);
	sti->num_blocked_thds++;
}

static inline void
tmem_add_to_gbl(struct spd_tmem_info *sti, unsigned int tid)
{
	struct blocked_thd *bthd;
	bthd = malloc(sizeof(struct blocked_thd));
	if (unlikely(bthd == NULL)) BUG();

	sti->num_glb_blocked++;
	bthd->thd_id = tid;
	bthd->spdid = sti->spdid;

	DOUT("Adding thd to the global blocked list: %d\n", bthd->thd_id);
	ADD_LIST(&global_blk_list, bthd, next, prev);
}

/* return 1 on success */
static inline int
remove_thd_from_blk_list(struct spd_tmem_info *sti, unsigned int tid)
{
	struct blocked_thd *bthd;
	for (bthd = FIRST_LIST(&sti->bthd_list, next, prev) ;
	     bthd != &sti->bthd_list ;
	     bthd = FIRST_LIST(bthd, next, prev)) {
		if (bthd->thd_id == tid) {
			REM_LIST(bthd, next, prev);
			free(bthd);
			sti->num_blocked_thds--;
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
tmem_thd_in_blk_list(struct spd_tmem_info *sti, unsigned int tid)
{
	struct blocked_thd *bthd;
	for (bthd = FIRST_LIST(&sti->bthd_list, next, prev) ;
	     bthd != &sti->bthd_list ;
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
tmem_reset_stats(struct spd_tmem_info *sti)
{
	int i;
	BUG();
	for (i = 0 ; i < MAX_NUM_THREADS ; i++) {
		sti->nthd_blks[i] = 0;
		sti->thd_blk_tot[i] = 0;
		sti->thd_blk_start[i] = 0;
	}
	for (i = 0 ; i < MAX_BLKED ; i++) {
		sti->stat_thd_blk[i] = 0;
	}
}

static inline int 
tmem_spd_concurrency_estimate(spdid_t spdid)
{
	struct spd_tmem_info *sti;
	tmem_item *tmi;
	unsigned int i, avg, touched_in_last_period = 0;
	unsigned long tot = 0, cnt = 0;
	
	TAKE();
	sti = get_spd_info(spdid);
	if (!sti || !SPD_IS_MANAGED(sti)) {
		goto err;
	}

	DOUT("sti->num_allocated %d sti->num_desired %d\n",sti->num_allocated, sti->num_desired);

//	if (sti->num_allocated < sti->num_desired && !sti->num_waiting_thds) {

	for (tmi = FIRST_LIST(&sti->tmem_list, next, prev) ; 
	     tmi != &sti->tmem_list ; 
	     tmi = FIRST_LIST(tmi, next, prev)) 
		if (TMEM_TOUCHED(tmi)) touched_in_last_period++;

	if (touched_in_last_period < sti->num_desired 
	     && !sti->num_waiting_thds) {
		assert(!SPD_HAS_BLK_THD(sti));
		avg = touched_in_last_period;
		goto done;
	}

	for (i = 0 ; i < MAX_BLKED ; i++) {
		int n = sti->stat_thd_blk[i];

		tot += (n * i);
		cnt += n;
		sti->stat_thd_blk[i] = 0;
	}

	if (cnt == 0 && sti->num_waiting_thds == 0) {
		avg = sti->num_allocated;
	} else {
		unsigned int blk_hist;

		if (cnt) blk_hist = (tot/cnt) + !(tot%cnt == 0); /* adjust for rounding */
		else     blk_hist = 0;

		avg = sti->num_allocated + (blk_hist > sti->num_waiting_thds ? 
					    blk_hist : sti->num_waiting_thds); 
		/* concurrency of self-suspension components can be estimated
		 * more accurately using ss counter */
		if (sti->ss_counter && avg < (sti->ss_counter + 1))
			avg = sti->ss_counter + 1;
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
	/* int i; */
	/* for (i = 0 ; i < MAX_NUM_SPDS ; i++) { */
	/* 	spdid_t spdid = i; */
	/* 	if (spd_tmem_info_list[spdid].managed == 0) continue; */
	/* 	printc("spd %d, allocated %d, desired %d, blk thds %d, glb %d, ss_counter %d\n", */
	/* 	       spd_tmem_info_list[spdid].spdid, */
	/* 	       spd_tmem_info_list[spdid].num_allocated, */
	/* 	       spd_tmem_info_list[spdid].num_desired, */
	/* 	       spd_tmem_info_list[spdid].num_blocked_thds, */
	/* 	       spd_tmem_info_list[spdid].num_glb_blocked, */
	/* 	       spd_tmem_info_list[spdid].ss_counter); */
	/* } */

	/* stkmgr_print_ci_freelist(); */
	printc("MGR %ld -> allocated: %d,\n", cos_spd_id(), tmems_allocated);
	RELEASE();
}

static inline int
tmem_set_suspension_limit(spdid_t cid, int max)
{
	struct spd_tmem_info *sti;
	TAKE();
	sti = get_spd_info(cid);
	if (!sti || !SPD_IS_MANAGED(sti)) goto err;
	if (max < 0) goto err;

	sti->ss_max = max;

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
	struct spd_tmem_info * sti = get_spd_info(cid);
	unsigned int counter = sti->ss_counter;
	/* +3 for round up. right shift 2 to decrease 25% */
	if (reset) sti->ss_counter -= (sti->ss_counter + 3) >> 2;
	RELEASE();
	return counter;
}

static inline int
tmem_set_over_quota_limit(int limit)
{
	TAKE();
	if (limit > MAX_NUM_MEM - tmems_target || limit < 0) {
		DOUT("Over-quota limit greater than global available quota. limit: %d.\n", limit);
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
	struct spd_tmem_info *sti;
	int ret;
	TAKE();
	sti = get_spd_info(cid);
	if (!sti || !SPD_IS_MANAGED(sti)) {
		RELEASE();
		return -1;
	}
	ret = sti->num_allocated;
	RELEASE();
	return ret;
}

static inline unsigned long
tmem_get_thd_blk_time(unsigned short int tid, spdid_t spdid, int reset)
{
	struct spd_tmem_info *sti;
	unsigned long ret;
	long long a = 0;
	u64_t t;

	TAKE();
	sti = get_spd_info(spdid);
	if (!sti || !SPD_IS_MANAGED(sti) || tid >= MAX_NUM_THREADS) {
		goto err;
	}

	/* currently blocked? */
	if (sti->thd_blk_start[tid]) {
		rdtscll(t);
		a += t - sti->thd_blk_start[tid];
	}
	if (sti->nthd_blks[tid]) {
		a = (a + sti->thd_blk_tot[tid])/sti->nthd_blks[tid];
	} 
	if (reset) {
		sti->thd_blk_tot[tid] = 0;
		sti->nthd_blks[tid] = 0;
	}
//	ret = (a >> 20) + ! ((a & 1048575) == 0);/* right shift 20 bits and round up, 2^20 - 1 = 1048575 */
	ret = (a >> 8) + 1;//! ((a & 1048575) == 0);

	RELEASE();

	return ret;
err:
	RELEASE();
	return -1;
}

static inline int
tmem_get_thd_blk_cnt(unsigned short int tid, spdid_t spdid, int reset)
{
	struct spd_tmem_info *sti;
	int n;

	TAKE();
	sti = get_spd_info(spdid);
	if (!sti || !SPD_IS_MANAGED(sti) || tid >= MAX_NUM_THREADS) {
		goto err;
	}

	n = sti->nthd_blks[tid];
	/* Thread on the blocked list? */
	if (sti->thd_blk_start[tid] && n == 0) n = 1;
	if (reset) {
		sti->thd_blk_tot[tid] = 0;
		sti->nthd_blks[tid] = 0;
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
	struct spd_tmem_info *sti;
	int i;

	TAKE();
	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		sti = get_spd_info(i);
		if (!sti) BUG();
		if (!SPD_IS_MANAGED(sti)) continue;
		
		tmem_reset_stats(sti);
	}
	RELEASE();
}

#endif
