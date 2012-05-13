#include <cos_component.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_list.h>
#include <cos_debug.h>

#include <sched.h>

#include <tmem.h>
#include <mem_pool.h>

struct spd_tmem_info *
get_spd_info(spdid_t spdid)
{
	struct spd_tmem_info *sti;

	assert(spdid < MAX_NUM_SPDS);
	sti = &spd_tmem_info_list[spdid];
	
	return sti;
}

int
put_mem(tmem_item *tmi)
{
	assert(EMPTY_LIST(tmi, next, prev));
	assert(tmi->parent_spdid == 0);
	tmems_allocated--;
	if (tmems_allocated > tmems_target) {
		RELEASE();
		mempool_put_mem(cos_spd_id(), LOCAL_ADDR(tmi));
		TAKE();
		free_item_data_struct(tmi);
	} else {
		tmi->free_next = free_tmem_list;
		free_tmem_list = tmi;
		if (GLOBAL_BLKED)
			wake_glb_blk_list(0);
	}	

	return 0;
}

tmem_item *
get_mem(void)
{
	tmem_item *tmi;
	void *l_addr;

	/* Do we need to maintain global stack target? If we set a
	 * limit to each component, can we get a global target as
	 * well? Disable this first because it prevents
	 * self-suspension stacks over-quota allocation, which is
	 * necessary
	 */
	/* if (stacks_allocated >= stacks_target) return NULL; */

	tmi = free_tmem_list;

	if (tmi) {
		free_tmem_list = tmi->free_next;
	} else {
		RELEASE();
		l_addr = mempool_get_mem(cos_spd_id(), 1);
		TAKE();
		if (l_addr) tmi = alloc_item_data_struct(l_addr);
	}

	if (!tmi) return NULL;

	tmems_allocated++;

	return tmi;
}

void event_waiting()
{
	while (1) {
		mempool_tmem_mgr_event_waiting(cos_spd_id());
		TAKE();
		wake_glb_blk_list(0);
		RELEASE();
	}
	DOUT("Event thread terminated!\n");
	BUG();
	return;
}

inline void tmem_mark_relinquish_all(struct spd_tmem_info *sti)
{
	struct cos_component_information *spd_c_info;
	
	int spd_id;
	spd_id = cos_spd_id();

	spd_c_info = sti->ci.spd_cinfo_page;
	assert(spd_c_info);

	spd_c_info->cos_tmem_relinquish[TMEM_RELINQ] = 1;
	sti->relinquish_mark = 1;
	
	return;
}


inline void tmem_unmark_relinquish_all(struct spd_tmem_info *sti)
{
	struct cos_component_information *spd_c_info;

	int spd_id;
	spd_id = cos_spd_id();
	
	spd_c_info = sti->ci.spd_cinfo_page;
	assert(spd_c_info);

	spd_c_info->cos_tmem_relinquish[TMEM_RELINQ] = 0;
	sti->relinquish_mark = 0;
	
	return;
}


inline int
tmem_wait_for_mem_no_dependency(struct spd_tmem_info *sti)
{
	/* Following are not ture for cbufs now, do we need to ensure
	 * these? */
	/* assert(sti->num_allocated > 0); */
	/* assert(sti->ss_counter); */

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
tmem_wait_for_mem(struct spd_tmem_info *sti)
{
	unsigned int i = 0;

	/* How to ensure one item per component? */
	/* assert(sti->num_allocated > 0); */
	
	int ret, dep_thd, in_blk_list;
	do {
		dep_thd = resolve_dependency(sti, i++); 

		if (i - 1 > sti->ss_counter) {
			sti->ss_counter = i - 1; /* update self-suspension counter */
			DOUT("sti->allcoated %d, sti->desired %d, dep_thd %d!!,curr %d\n",sti->num_allocated, sti->num_desired,dep_thd, cos_get_thd_id());
		}

		/* dep_thd == 0 means the tmem owner is the current
		 * thd, try next tmem. -2 means found local cache. we
		 * should try to use it */
		if (dep_thd == 0 || tmem_thd_in_blk_list(sti, dep_thd)) {
			DOUT("dep_thd %d, in_blk %d\n", dep_thd, tmem_thd_in_blk_list(sti, dep_thd));
			in_blk_list = 1;
			continue;
		}
		if (dep_thd == -2) {
			remove_thd_from_blk_list(sti, cos_get_thd_id());
			break;
		}

		if (dep_thd == -1) {
			printc("MGR %ld: Self-suspension detected(cnt:%d)! comp: %d, thd:%d, waiting:%d desired: %d alloc:%d\n",
			       cos_spd_id(), sti->ss_counter,sti->spdid, cos_get_thd_id(), sti->num_waiting_thds, sti->num_desired, sti->num_allocated);
			if (i > sti->ss_counter) sti->ss_counter = i;

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
		 * Pasting multiple arguments to sched_block (i.e. all
		 * threads that have tmems in the component) will
		 * make this algorithm correct, but we want tmem/idl
		 * support to implement that.
		 */
		DOUT("MGR %ld >>> %d try to depend on %d comp %d i%d\n", cos_spd_id(), cos_get_thd_id(), dep_thd, sti->spdid, i);

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
		in_blk_list = tmem_thd_in_blk_list(sti, cos_get_thd_id());
			
		if (in_blk_list) {
			assert(ret < 0);
			sched_wakeup(cos_spd_id(), cos_get_thd_id());
		}
		DOUT("%d finished depending on %d. comp %d. i %d. ss_cnt %d. ret %d\n",
		       cos_get_thd_id(), dep_thd, sti->spdid,i,sti->ss_counter, ret);

	} while (in_blk_list);
	DOUT("Thd %d wokeup and is obtaining a tmem\n", cos_get_thd_id());

	return 1;
}

inline tmem_item *
tmem_grant(struct spd_tmem_info *sti)
{
	tmem_item *tmi = NULL, *local_cache = NULL;
	int eligible, meas = 0;

	sti->num_waiting_thds++;

	/* 
	 * Is there a tmem in the local freelist? If not, is there
	 * one in the global freelist and we can ensure that there are
	 * enough tmems for the empty components, and we are under
	 * quota on tmems? Otherwise block!
	 */

	while (1) {
		tmi = free_mem_in_local_cache(sti);
		if (tmi) {
			local_cache = tmi;
			DOUT("found one cached!! \n");
			break;
		}

		eligible = 0;

		DOUT("thd %d  spd %d sti->num_allocated %d sti->num_desired %d\n",cos_get_thd_id(), sti->spdid, sti->num_allocated, sti->num_desired);
		DOUT("empty_comps %d (MAX_NUM_MEM - tmems_allocated) %d\n",empty_comps , (MAX_NUM_MEM - tmems_allocated));

		if (sti->num_allocated < sti->num_desired &&
		    (empty_comps < (MAX_NUM_MEM - tmems_allocated) || sti->num_allocated == 0)) {
			/* DOUT("alloooooooooo!!\n"); */
			/* We are eligible for allocation! */
			eligible = 1;
			tmi = get_mem();
			if (tmi) break;
		}
		if (!meas) {
			meas = 1;
			tmem_update_stats_block(sti, cos_get_thd_id());
		}
		/* DOUT("In tmem_grant:: mem in %d set to relinquish, %d waiting\n", sti->spdid, cos_get_thd_id()); */

		/* 
		 * gap: relinquish is NOT really used right now to
		 * facilitate the priority inheritance.  Instead of
		 * making the conditions for relinquish only based on
		 * desired and allocated, lets try and be smarter:  

		 * if desired < allocated || HP thread for stack ->
		 * marked relinquished

		 * if desired >= allocated && no (HP) threads waiting
		 * for stack in this component -> UNmarked relinquished

		 * OR 

		 * 1) threads waiting (from this component, either on the
		 * local or global blocked list) -> marked relinquish

		 * 2) desired < allocated -> marked relinquish

		 * OTHERWISE -> unmarked relinquished

		 * We want two functions:

		 * tmem_should_mark_relinquish(sti) -> should the tmem in
		 * component sti->spdid be marked as reqlinquish???

		 * tmem_should_unmark_relinquish(sti) -> should the tmem in
		 * component sti->spdid be unmarked as reqlinquish???

		 * int tmem_should_unmark_relinquish(sti) { return !tmem_should_mark_relinquish(sti); }



		 * when we will carry out an action that could provide a tmem:
		 * if tmem_should_mark_relinquish(...) marked = 1
		 * ...
		 * if tmem_should_unmark_relinquish(...) && marked -> spd_unmark_relinquish()

		 * when we will carry out an action that could require a tmem:
		 * if tmem_should_unmark_relinquish(...) unmarked = 1
		 * ...
		 * if tmem_should_mark_relinquish(...) && unmarked -> spd_mark_relinquish()

		 */

		if (eligible) {
			tmem_add_to_gbl(sti, cos_get_thd_id());
		        /* 
			 * gap: come back to this -- problem is that a
			 * high priority thread can block on the
			 * global list, but a thread in the component
			 * that sees the reqlinquish will _not_ wake
			 * up threads on the global list in
			 * cbuf_c_delete...  Possibly solve this by
			 * waking up threads on the global list that
			 * are waiting for this component. 
			 */
		} else {
			tmem_add_to_blk_list(sti, cos_get_thd_id());
		}

		/* spd_mark_relinquish(sti); */
		if (tmem_should_mark_relinquish(sti))
			tmem_mark_relinquish_all(sti);

		/* Priority-Inheritance */
		if (tmem_wait_for_mem(sti) == 0) {
			assert(sti->ss_counter);
			DOUT("self...\n");
			/* We found self-suspension. Are we eligible
			 * for tmems now? If still not, block
			 * ourselves without dependencies! */
			if (sti->num_allocated < (sti->num_desired + sti->ss_max) &&
			    over_quota_total < over_quota_limit &&
			    (empty_comps < (MAX_NUM_MEM - tmems_allocated) || sti->num_allocated == 0)) {

				/* DOUT("when self:: num_allocated %d num_desired+max %d\n",sti->num_allocated, sti->num_desired + sti->ss_max);				 */
				tmi = get_mem();
				if (tmi) {
					DOUT(" got tmi!!!\n");
					/* remove from the block list before grant */
					remove_thd_from_blk_list(sti, cos_get_thd_id());
					break;
				}
			}
			tmem_wait_for_mem_no_dependency(sti);
		}
                /* Wake up others here. if we are the highest priority
		 * thd, we need to wake up other blked thds! */
		tmem_spd_wake_threads(sti);
		/* Make sure we call the wake function above. Don't
		 * use "if blked then call wake" here because we also
		 * check if we should clear relinquish bit in it. Call
		 * it and it will do the correct logic.
		 */
	}

	if (!local_cache) {
		mgr_map_client_mem(tmi, sti); 
		DOUT("Adding to local tmem_list\n");
		ADD_LIST(&sti->tmem_list, tmi, next, prev);
		sti->num_allocated++;
		if (sti->num_allocated == 1) empty_comps--;
		if (sti->num_allocated > sti->num_desired) over_quota_total++;
		assert(sti->num_allocated == tmem_num_alloc_tmems(sti->spdid));
	}

	if (meas) tmem_update_stats_wakeup(sti, cos_get_thd_id());

	sti->num_waiting_thds--;

	DOUT("Granted: num_allocated %d num_desired %d\n",sti->num_allocated, sti->num_desired);

	return tmi;
}


inline void
get_mem_from_client(struct spd_tmem_info *sti)
{
	//DOUT("calling into get_mem_from_cli\n");
	tmem_item * tmi;
	while (sti->num_desired < sti->num_allocated) {
		DOUT("get_mem_from cli\n");
		tmi = mgr_get_client_mem(sti);
		if (!tmi) 
			break;
		put_mem(tmi);
	}

	/* if we haven't harvested enough tmems, do so lazily */
	/* if (sti->num_desired < sti->num_allocated) spd_mark_relinquish(sti); */
	// Jiguo: This is used for policy, so should_mark_relinquish is not used here	
	if (sti->num_desired < sti->num_allocated)
		tmem_mark_relinquish_all(sti);
}

inline void
return_tmem(struct spd_tmem_info *sti)
{
	spdid_t s_spdid;

	assert(sti);
	s_spdid = sti->spdid;
	DOUT("spd %d: return_mem is called \n", s_spdid);
	DOUT("Before:: num_allocated %d num_desired %d\n",sti->num_allocated, sti->num_desired);
	
        /* if (sti->num_desired < sti->num_allocated || sti->num_glb_blocked) { 2nd condition is used for max pool testing */
	if (sti->num_desired < sti->num_allocated) {   // only blocked on glb for other spds
		get_mem_from_client(sti);
	}

	if (SPD_HAS_BLK_THD(sti) || SPD_HAS_BLK_THD_ON_GLB(sti)) {
		/* Here we release the lock then wake up the highest
		 * priority thread. This is a more efficient way to
		 * wake up higher priority threads. */
		tmem_spd_wake_first_thread(sti);
	}

	/* below assert is not true as we might release lock when
	 * waking up other threads. */
	/* assert(!SPD_HAS_BLK_THD(sti) && !SPD_HAS_BLK_THD_ON_GLB(sti)); */

	DOUT("After return called:: num_allocated %d num_desired %d\n",sti->num_allocated, sti->num_desired);
}

/**
 * Remove all free cache from client. Only called by set_concurrency
 * when using memory pool policy.
 */
static inline void
remove_spare_cache_from_client(struct spd_tmem_info *sti)
{
	tmem_item * tmi;

	/* printc("in spd %ld\n", sti->spdid); */

	while (1) {
		tmi = mgr_get_client_mem(sti);
		if (!tmi)
			return;
		DOUT("In %d found tmem to be allocated  %d\n\n", sti->spdid, sti->num_allocated);
		put_mem(tmi);
		DOUT("remove spare----\n");
	}
	return;
}

/**
 * returns 0 on success
 */
inline int
tmem_set_concurrency(spdid_t spdid, int concur_lvl, int remove_spare)
{
	struct spd_tmem_info *sti;
	int diff, old;

	TAKE();
	sti = get_spd_info(spdid);	

	/* if (concur_lvl > 1) DOUT("Set concur of %d to %d\n", spdid, concur_lvl); */
	//DOUT("\n<<<tmem::Set concur of %d to %d>>>\n", spdid, concur_lvl);
	if (!sti || !SPD_IS_MANAGED(sti)) goto err;
	if (concur_lvl < 0) goto err;

	old = sti->num_desired;
	sti->num_desired = concur_lvl;
	tmems_target += concur_lvl - old;

	/* update over-quota allocation counter */
	if (old < (int)sti->num_allocated) 
		over_quota_total -= (concur_lvl <= (int)sti->num_allocated) ? concur_lvl - old : (int)sti->num_allocated - old;
	else if (concur_lvl < (int)sti->num_allocated)
		over_quota_total += sti->num_allocated - concur_lvl;
	
	diff = sti->num_allocated - sti->num_desired;
	if (diff > 0) get_mem_from_client(sti);
	if (diff < 0 && SPD_HAS_BLK_THD(sti)) {
		sti->wake_up_epoch++;
		tmem_spd_wake_threads(sti);
		/* No need to use the wake_first here. Only increment the
		   epoch counter. The set_tmem thd should has a higher
		   priority */ 
	}
	
	if (remove_spare)
		remove_spare_cache_from_client(sti);

	mgr_clear_touched_flag(sti);
	RELEASE();
	return 0;
err:
	RELEASE();
	return -1;

}
