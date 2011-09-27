#include <cos_component.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_list.h>

#include <sched.h>

#include <tmem.h>

struct spd_tmem_info *
get_spd_info(spdid_t spdid)
{
	struct spd_tmem_info *sti;

	assert(spdid < MAX_NUM_SPDS);
	sti = &spd_tmem_info_list[spdid];
	
	return sti;
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
	assert(sti->num_allocated > 0);
	assert(sti->ss_counter);

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

	assert(sti->num_allocated > 0);
	
	int ret, dep_thd, in_blk_list;
	do {
		dep_thd = resolve_dependency(sti, i); 

		if (i > sti->ss_counter) sti->ss_counter = i; /* update self-suspension counter */

		if (dep_thd == 0) {
			/* printc("Self-suspension detected(cnt:%d)! comp: %d, thd:%d, waiting:%d desired: %d alloc:%d\n", */
			/*        sti->ss_counter,sti->spdid, cos_get_thd_id(), sti->num_waiting_thds, sti->num_desired, sti->num_allocated); */
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
		 * Pasting multiple arguments to sched_block (i.e. all
		 * threads that have tmems in the component) will
		 * make this algorithm correct, but we want tmem/idl
		 * support to implement that.
		 */
		printc("%d try to depend on %d comp %d i%d\n", cos_get_thd_id(), dep_thd, sti->spdid, i);
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
		printc("%d finished depending on %d. comp %d. i %d. ss_cnt %d. ret %d\n",
		       cos_get_thd_id(), dep_thd, sti->spdid,i,sti->ss_counter, ret);
		i++;
	} while (in_blk_list);
	printc("Thd %d wokeup and is obtaining a tmem\n", cos_get_thd_id());

	return 1;
}

// Before this function is called, threads are already on the block lists
inline int tmem_should_mark_relinquish(struct spd_tmem_info *sti)
{
	if (sti->num_allocated >= sti->num_desired 
	    || (empty_comps >= (MAX_NUM_ITEMS - tmems_allocated)))
		return 1;
	else
		return 0;
}

inline int tmem_should_unmark_relinquish(struct spd_tmem_info *sti)
{
	if (!SPD_HAS_BLK_THD(sti) && !SPD_HAS_BLK_THD_ON_GLB(sti)
	    && sti->num_desired >= sti->num_allocated)
		return 1;
	else
		return 0;
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

	/* For cbuf only
	 * find an unused cbuf_id
	 * looks through to find id
	 * if does not find, allocate a page for the meta data
	 */

	while (1) {
#ifdef MEM_IN_LOCAL_CACHE
		tmi = (tmem_item *)MEM_IN_LOCAL_CACHE(sti);
		if (tmi){
			local_cache = tmi;
			break;
		}
#endif
		DOUT("request tmem\n");
		/* printc(" \n ~~~ thd %d request tmem!! ~~~\n\n", cos_get_thd_id()); */
		eligible = 0;

		/* printc("thd %d  spd %ld sti->num_allocated %d sti->num_desired %d\n",cos_get_thd_id(), sti->spdid, sti->num_allocated, sti->num_desired); */
		/* printc("empty_comps %d (MAX_NUM_ITEMS - tmems_allocated) %d\n",empty_comps , (MAX_NUM_ITEMS - tmems_allocated)); */

		if (sti->num_allocated < sti->num_desired &&
		    (empty_comps < (MAX_NUM_ITEMS - tmems_allocated) || sti->num_allocated == 0)) {
			/* printc("alloooooooooo!!\n"); */
			/* We are eligible for allocation! */
			eligible = 1;
			tmi = get_mem();
			if (tmi) break;
		}
		if (!meas) {
			meas = 1;
			tmem_update_stats_block(sti, cos_get_thd_id());
		}
		/* printc("In tmem_grant:: mem in %d set to relinquish, %d waiting\n", sti->spdid, cos_get_thd_id()); */

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

		/* /\* Priority-Inheritance *\/ */
		if (tmem_wait_for_mem(sti) == 0) {
			assert(sti->ss_counter);
			printc("self...\n");
			/* We found self-suspension. Are we eligible
			 * for tmems now? If still not, block
			 * ourselves without dependencies! */
			if (sti->num_allocated < (sti->num_desired + sti->ss_max) &&
			    over_quota_total < over_quota_limit &&
			    (empty_comps < (MAX_NUM_ITEMS - tmems_allocated) || sti->num_allocated == 0)) {

				printc("when self:: num_allocated %d num_desired+max %d\n",sti->num_allocated, sti->num_desired + sti->ss_max);				
				tmi = get_mem();
				if (tmi) {
					printc(" got tmi!!!\n");
					/* remove from the block list before grant */
					remove_thd_from_blk_list(sti, cos_get_thd_id());
					break;
				}
			}
			tmem_wait_for_mem_no_dependency(sti);
		}
	}

	if (!local_cache) {
		mgr_map_client_mem(tmi, sti); 
		DOUT("Adding to local spdid list\n");
		ADD_LIST(&sti->tmem_list, tmi, next, prev);
		sti->num_allocated++;
		if (sti->num_allocated == 1) empty_comps--;
		if (sti->num_allocated > sti->num_desired) over_quota_total++;
		assert(sti->num_allocated == tmem_num_alloc_tmems(sti->spdid));
	}

	if (meas) tmem_update_stats_wakeup(sti, cos_get_thd_id());

	sti->num_waiting_thds--;

	/* printc("Granted: num_allocated %d num_desired %d\n",sti->num_allocated, sti->num_desired); */

	return tmi;
}


inline void
get_mem_from_client(struct spd_tmem_info *sti)
{
	//printc("calling into get_mem_from_cli\n");
	tmem_item * tmi;
	while (sti->num_desired < sti->num_allocated) {
		/* printc("get_mem_from cli\n"); */
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
	/* printc("return_mem is called \n"); */
	/* printc("Before:: num_allocated %d num_desired %d\n",sti->num_allocated, sti->num_desired); */
	
        /* if (sti->num_desired < sti->num_allocated || sti->num_glb_blocked) { 2nd condition is used for max pool testing */
	if (sti->num_desired < sti->num_allocated) {   // only blocked on glb for other spds
		printc("fly..............\n");
		get_mem_from_client(sti);
	}
	if (SPD_HAS_BLK_THD(sti) || SPD_HAS_BLK_THD_ON_GLB(sti))
		tmem_spd_wake_threads(sti);

	assert(!SPD_HAS_BLK_THD(sti) && !SPD_HAS_BLK_THD_ON_GLB(sti));

	if (tmem_should_unmark_relinquish(sti) && sti->relinquish_mark == 1) 
		tmem_unmark_relinquish_all(sti);

	/* printc("After return called:: num_allocated %d num_desired %d\n",sti->num_allocated, sti->num_desired); */

}

/**
 * Remove all free cache from client. Only called by set_concurrency.
 */
static inline void
remove_spare_cache_from_client(struct spd_tmem_info *sti)
{
	tmem_item * tmi;
	while (1) {
		tmi = mgr_get_client_mem(sti);
		if (!tmi)
			return;
		put_mem(tmi);
		/* printc("remove spare----\n"); */
	}
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

	/* if (concur_lvl > 1) printc("Set concur of %d to %d\n", spdid, concur_lvl); */
	//printc("\n<<<tmem::Set concur of %d to %d>>>\n", spdid, concur_lvl);
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
	if (diff < 0 && SPD_HAS_BLK_THD(sti)) tmem_spd_wake_threads(sti);
	if (remove_spare) remove_spare_cache_from_client(sti);
	RELEASE();
	return 0;
err:
	RELEASE();
	return -1;

}
