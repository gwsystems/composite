#include <cos_component.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_list.h>
#include <cos_vect.h>

#include <sched.h>

#include <tmem.h>

inline struct spd_stk_info *
get_spd_info(spdid_t spdid)
{
	struct spd_stk_info *ssi;

	if (spdid >= MAX_NUM_SPDS) BUG();
	ssi = &spd_stk_info_list[spdid];
	
	return ssi;
}

inline int
tmem_wait_for_mem_no_dependency(struct spd_stk_info *ssi)
{
	assert(ssi->num_allocated > 0);
	assert(ssi->ss_counter);
	DOUT("request_stack\n");
	spd_mark_relinquish(ssi->spdid);

	DOUT("All stacks for %d set to relinquish, %d waiting\n", ssi->spdid, cos_get_thd_id());
	
	tmem_add_to_blk_list(ssi, cos_get_thd_id());
	DOUT("Blocking thread: %d\n", bthd->thd_id);

	RELEASE();
	sched_block(cos_spd_id(), 0);
	TAKE();

	DOUT("Thd %d wokeup and is obtaining a stack\n", cos_get_thd_id());
	return 1;
}

/**
 * Asks for a stack back from all of the components.  Will release and
 * take the lock.
 */
inline int
tmem_wait_for_mem(struct spd_stk_info *ssi)
{
	unsigned int i = 0;

	assert(ssi->num_allocated > 0);
	DOUT("request_stack\n");
	spd_mark_relinquish(ssi->spdid);

	DOUT("All stacks for %d set to relinquish, %d waiting\n", ssi->spdid, cos_get_thd_id());
	
	tmem_add_to_blk_list(ssi, cos_get_thd_id());

	int ret, dep_thd, in_blk_list;
	do {
		dep_thd = resolve_dependency(ssi, i); 
		if (i > ssi->ss_counter) ssi->ss_counter = i; /* update self-suspension counter */

		if (dep_thd == 0) {
			/* printc("Self-suspension detected(cnt:%d)! comp: %d, thd:%d, waiting:%d desired: %d alloc:%d\n", */
			/*        ssi->ss_counter,ssi->spdid, cos_get_thd_id(), ssi->num_waiting_thds, ssi->num_desired, ssi->num_allocated); */
			/* remove from the block list before return */
			ret = remove_thd_from_blk_list(ssi, cos_get_thd_id());
			assert(ret == 0);
			assert(i > 0);
			return 0;
		}

		/* 
		 * FIXME: We really need to pass multiple arguments to
		 * the sched_block function.  We want sched block to
		 * choose any one of the threads that is not blocked
		 * as a dependency.  As it stands now, if we are
		 * preempted between the RELEASE and the TAKE, the
		 * stack list can change, giving us inconsistent
		 * results iff we are preempted and the list changes.
		 * Passing multiple arguments to sched_block (i.e. all
		 * threads that have stacks in the component) will
		 * make this algorithm correct, but we want cbuf/idl
		 * support to implement that.
		 */
		//printc("%d try to depend on %d comp %d i%d\n", cos_get_thd_id(), dep_thd, ssi->spdid, i);
		RELEASE();
		ret = sched_block(cos_spd_id(), dep_thd);
		TAKE(); 

		/* 
		 * STKMGR: self wakeup
		 *
		 * Note that the current thread will call
		 * wakeup on ourselves here.  First, the
		 * common case where sched_block does not
		 * return -1: This happens because 1)
		 * stkmgr_wait_for_stack has placed this
		 * thread on a blocked list, then called
		 * sched_block, 2) another thread (perhaps the
		 * depended on thread) might find this thread
		 * in the block list and wake it up (thus
		 * correctly modifying the block counter in
		 * the scheduler).  HOWEVER, if in step 1),
		 * the dependency cannot be made (because the
		 * depended on thread is blocked) and
		 * sched_block returns -1, then we end up
		 * modifying the block counter in the
		 * scheduler, and if we don't later decrement
		 * it, then it is inconsistent.  Thus we call
		 * sched_wakeup on ourselves.
		 */
		in_blk_list = tmem_thd_in_blk_list(ssi, cos_get_thd_id());
			
		if (in_blk_list) {
			assert(ret < 0);
			if (ssi->spdid != 12) {
				printc("thd %d spdid %d, dep_thd %d\n",cos_get_thd_id(), ssi->spdid, dep_thd);
				assert(0);
			}
			sched_wakeup(cos_spd_id(), cos_get_thd_id());
		}
		/* printc("%d finished depending on %d. comp %d. i %d. cnt %d. ret %d.on block list? %d\n", */
		/*        cos_get_thd_id(), dep_thd, ssi->spdid,i,ssi->ss_counter, ret); */
		i++;
	} while (in_blk_list);
	DOUT("Thd %d wokeup and is obtaining a stack\n", cos_get_thd_id());

	return 1;
}

tmem_item *
tmem_contend_mem(struct spd_stk_info *ssi)
{
	tmem_item *tmi = NULL;
	int meas = 0;

	ssi->num_waiting_thds++;
	/* 
	 * Is there a stack in the local freelist? If not, is there
	 * one in the global freelist and there are enough stacks for
	 * the empty components, and we are under quota on stacks or
	 * the destination component is self-suspension? Otherwise
	 * block!
	 */

	while (1) {
#ifdef CHECK_LOCAL_CACHE
		if (CHECK_LOCAL_CACHE(ssi)) break;
#endif
		if ((empty_comps < (MAX_NUM_STACKS - stacks_allocated) || ssi->num_allocated == 0)
		    && ssi->num_allocated < ssi->num_desired
		    && NULL != (tmi = freelist_remove())) {
			add_tmem_to_spd(tmi, ssi);
			spd_freelist_add(ssi->spdid, tmi);
			break;
		}
		if (!meas) {
			meas = 1;
			tmem_update_stats_block(ssi, cos_get_thd_id());
		}
		DOUT("Stack list is null, we need to revoke a stack: spdid: %d thdid: %d\n",
		     d_spdid,
		     cos_get_thd_id());
		if (tmem_wait_for_mem(ssi) == 0) {
			assert(ssi->ss_counter);
			/* We found self-suspension. Are we eligible
			 * for stacks now? If not, block ourselves
			 * without dependencies! */
			if (empty_comps < (MAX_NUM_STACKS - stacks_allocated) &&
			    ssi->num_allocated < (ssi->num_desired + ssi->ss_max) &&
			    over_quota < over_quota_limit && NULL != (tmi = freelist_remove())){ 
				add_tmem_to_spd(tmi, ssi);
				spd_freelist_add(ssi->spdid, tmi);
				break;
			} else 
				tmem_wait_for_mem_no_dependency(ssi);
		}
	}
	if (meas) tmem_update_stats_wakeup(ssi, cos_get_thd_id());

	ssi->num_waiting_thds--;

	return tmi;
}

/* data structure independent function */
/* 
 * Is there a component with blocked threads?  Which is the one with
 * the largest disparity between the number of stacks it has, and the
 * number it is supposed to have?
 */
inline struct spd_stk_info *
find_spd_requiring_stk(void)
{
	int i, max_required = 0;
	struct spd_stk_info *best = NULL;

	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		struct spd_stk_info *ssi = &spd_stk_info_list[i];
		if (!SPD_IS_MANAGED(ssi)) continue;

		/* priority goes to spds with blocked threads */
		if (SPD_HAS_BLK_THD(ssi) && ssi->num_desired > ssi->num_allocated) {
			int diff = ssi->num_desired - ssi->num_allocated;

			if (max_required < diff) {
				best = ssi;
				max_required = diff;
			}
		}
	}
	return best;
}

int
tmem_find_home(tmem_item *tmi, struct spd_stk_info *prev)
{
	struct spd_stk_info *dest;

	assert(EMPTY_LIST(tmi, next, prev));
	dest = find_spd_requiring_stk();
	if (!dest) {
		freelist_add(tmi);
	} else {
		assert(SPD_HAS_BLK_THD(dest));
		add_tmem_to_spd(tmi, dest);
		spd_freelist_add(dest->spdid, tmi);
		tmem_spd_wake_threads(dest);
	}
	return 0;
}

void 
tmem_remove_and_find_home(struct spd_stk_info *ssi, tmem_item *tmi)
{
	remove_tmem_from_spd(tmi, ssi);
	tmem_find_home(tmi, ssi);
}

/**
 * Give a tmem item back to the mgr and to see if we should assign it
 * somewhere.  Assume that the tmem item is NOT on the component's
 * freelist.
 */
void
return_tmem(struct spd_stk_info *ssi, tmem_item *tmi)
{
	spdid_t s_spdid;

	assert(tmi && ssi);
	s_spdid = ssi->spdid;
	/* Don't move the stack if it should be here! */
	if (ssi->num_desired >= ssi->num_allocated) {
		/* restore in component's freelist */
		spd_freelist_add(s_spdid, tmi);
		/* wake threads! */
		tmem_spd_wake_threads(ssi);
		if (!SPD_HAS_BLK_THD(ssi)) {
			/* we're under or at quota, and there are no
			 * blocked threads, no more relinquishing! */
			spd_unmark_relinquish(ssi);
		}
	} else {
		tmem_remove_and_find_home(ssi, tmi);
		/* wake threads! */
		tmem_spd_wake_threads(ssi);
	}
}

int 
spd_remove_spare(struct spd_stk_info *ssi)
{
	tmem_item *tmi;

	tmi = spd_freelist_remove(ssi->spdid);
	if (!tmi) return -1;
	tmem_remove_and_find_home(ssi, tmi);

	return 0;
}

/**
 * returns 0 on success
 */
int
revoke_mem_from(spdid_t spdid)
{
	tmem_item *tmi;
	struct spd_stk_info *ssi;

	ssi = get_spd_info(spdid);

	/* Is there a stack on the component's freelist? */
	tmi = spd_freelist_remove(spdid);
	if(tmi == NULL) return -1;

	return_tmem(ssi, tmi);
	
	return 0;
}

void 
spd_remove_mem(spdid_t spdid, unsigned int n_pages)
{
	while (n_pages && !revoke_mem_from(spdid)) {
		//printc(">>> found and removed stack from %d (tid %d)\n", spdid, cos_get_thd_id());
		n_pages--;
	}
	/* if we haven't harvested enough stacks, do so lazily */
	if (n_pages) spd_mark_relinquish(spdid);
}

/**
 * returns 0 on success
 */
inline int
tmem_set_concurrency(spdid_t spdid, int concur_lvl, int remove_spare)
{
	struct spd_stk_info *ssi;
	int diff, old;

	TAKE();
	ssi = get_spd_info(spdid);	

	/* if (concur_lvl > 1) printc("Set concur of %d to %d\n", spdid, concur_lvl); */
	if (!ssi || !SPD_IS_MANAGED(ssi)) goto err;
	if (concur_lvl < 0) goto err;

	old = ssi->num_desired;
	ssi->num_desired = concur_lvl;
	stacks_target += concur_lvl - old;

	/* update over-quota allocation counter */
	if (old < (int)ssi->num_allocated) 
		over_quota -= (concur_lvl <= (int)ssi->num_allocated) ? concur_lvl - old : (int)ssi->num_allocated - old;
	else if (concur_lvl < (int)ssi->num_allocated)
		over_quota += ssi->num_allocated - concur_lvl;

	diff = ssi->num_allocated - ssi->num_desired;
	if (diff > 0) spd_remove_mem(ssi->spdid, diff);
	if (diff < 0 && SPD_HAS_BLK_THD(ssi)) tmem_spd_wake_threads(ssi);
	if (remove_spare) while (!spd_remove_spare(ssi)) ;

	RELEASE();
	return 0;
err:
	RELEASE();
	return -1;

}
