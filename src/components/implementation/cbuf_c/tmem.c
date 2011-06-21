#include <cos_component.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_list.h>
//#include <cos_vect.h>

#include <sched.h>

#include <tmem.h>

struct spd_stk_info *
get_spd_info(spdid_t spdid)
{
	struct spd_stk_info *ssi;

	assert(spdid < MAX_NUM_SPDS);
	ssi = &spd_stk_info_list[spdid];
	
	return ssi;
}

inline int
tmem_wait_for_mem_no_dependency(struct spd_stk_info *ssi)
{
	assert(ssi->num_allocated > 0);
	assert(ssi->ss_counter);

	RELEASE();
	sched_block(cos_spd_id(), 0);
	TAKE();

	DOUT("Thd %d wokeup and is obtaining a stack\n", cos_get_thd_id());
	return 1;
}

/**
 * Implement PIP. Block current thread with dependency. Will release
 * and take the lock.
 */
inline int
tmem_wait_for_mem(struct spd_stk_info *ssi)
{
	unsigned int i = 0;

	assert(ssi->num_allocated > 0);
	
	int ret, dep_thd, in_blk_list;
	do {
		dep_thd = resolve_dependency(ssi, i); 
		if (i > ssi->ss_counter) ssi->ss_counter = i; /* update self-suspension counter */

		if (dep_thd == 0) {
			/* printc("Self-suspension detected(cnt:%d)! comp: %d, thd:%d, waiting:%d desired: %d alloc:%d\n", */
			/*        ssi->ss_counter,ssi->spdid, cos_get_thd_id(), ssi->num_waiting_thds, ssi->num_desired, ssi->num_allocated); */
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
				/* Remover Me: test for
				 * self-suspension when TE component
				 * id is 12 */
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

inline tmem_item *
tmem_grant(struct spd_stk_info *ssi)
{
	tmem_item *tmi = NULL;
	int eligible, meas = 0;

	ssi->num_waiting_thds++;

	/* 
	 * Is there a stack in the local freelist? If not, is there
	 * one in the global freelist and we can ensure that there are
	 * enough stacks for the empty components, and we are under
	 * quota on stacks? Otherwise block!
	 */

	while (1) {
#ifdef MEM_IN_LOCAL_CACHE
		if (MEM_IN_LOCAL_CACHE(ssi)) break;
#endif
		DOUT("request tmem\n");
		eligible = 0;
		if (ssi->num_allocated < ssi->num_desired &&
		    (empty_comps < (MAX_NUM_CBUFS - stacks_allocated) || ssi->num_allocated == 0)) {
			/* We are eligible for allocation! */
			eligible = 1;
			tmi = get_mem();
			if (tmi) break;
		}
		if (!meas) {
			meas = 1;
			tmem_update_stats_block(ssi, cos_get_thd_id());
		}
		DOUT("All mem for %d set to relinquish, %d waiting\n", ssi->spdid, cos_get_thd_id());
		spd_mark_relinquish(ssi->spdid);
		DOUT("Blocking thread: %d\n", bthd->thd_id);
		if (eligible)
			tmem_add_to_gbl(cos_get_thd_id());
		else
			tmem_add_to_blk_list(ssi, cos_get_thd_id());
		DOUT("Wait for mem: spdid: %d thdid: %d\n",
		     d_spdid,
		     cos_get_thd_id());
		/* Priority-Inheritance */
		if (tmem_wait_for_mem(ssi) == 0) {
			assert(ssi->ss_counter);
			/* We found self-suspension. Are we eligible
			 * for stacks now? If still not, block
			 * ourselves without dependencies! */
			if (ssi->num_allocated < (ssi->num_desired + ssi->ss_max) &&
			    over_quota_total < over_quota_limit &&
			    (empty_comps < (MAX_NUM_CBUFS - stacks_allocated) || ssi->num_allocated == 0)) {
				tmi = get_mem();
				if (tmi) {
					/* remove from the block list before grant */
					remove_thd_from_blk_list(ssi, cos_get_thd_id());
					break;
				}
			}
			tmem_wait_for_mem_no_dependency(ssi);
		}
	}
	
	if (tmi) mgr_map_client_mem(tmi, ssi); 

	if (meas) tmem_update_stats_wakeup(ssi, cos_get_thd_id());

	ssi->num_waiting_thds--;

	return tmi;
}

inline void
get_mem_from_client(struct spd_stk_info *ssi)
{
	tmem_item * tmi;
	while (ssi->num_desired < ssi->num_allocated) {
		tmi = mgr_get_client_mem(ssi);
		if (!tmi)
			break;
		put_mem(tmi);
	}
	/* if we haven't harvested enough stacks, do so lazily */
	if (ssi->num_desired < ssi->num_allocated) spd_mark_relinquish(ssi->spdid);
}

inline void
return_tmem(struct spd_stk_info *ssi)
{
	spdid_t s_spdid;

	assert(ssi);
	s_spdid = ssi->spdid;
	
	if (ssi->num_desired < ssi->num_allocated || GLOBAL_BLKED) {
		get_mem_from_client(ssi);
	}
	tmem_spd_wake_threads(ssi);
	if (!SPD_HAS_BLK_THD(ssi) && ssi->num_desired >= ssi->num_allocated) {
		/* we're under or at quota, and there are no
		 * blocked threads, no more relinquishing! */
		spd_unmark_relinquish(ssi);
	}
}

/**
 * Remove all free cache from client. Only called by set_concurrency.
 */
static inline void
remove_spare_cache_from_client(struct spd_stk_info *ssi)
{
	tmem_item * tmi;
	while (1) {
		tmi = mgr_get_client_mem(ssi);
		if (!tmi)
			return;
		put_mem(tmi);
	}
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
		over_quota_total -= (concur_lvl <= (int)ssi->num_allocated) ? concur_lvl - old : (int)ssi->num_allocated - old;
	else if (concur_lvl < (int)ssi->num_allocated)
		over_quota_total += ssi->num_allocated - concur_lvl;

	diff = ssi->num_allocated - ssi->num_desired;
	if (diff > 0) get_mem_from_client(ssi);
	if (diff < 0 && SPD_HAS_BLK_THD(ssi)) tmem_spd_wake_threads(ssi);
	if (remove_spare) remove_spare_cache_from_client(ssi);

	RELEASE();
	return 0;
err:
	RELEASE();
	return -1;

}
