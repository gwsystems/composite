#include <cos_scheduler.h>

/**************** Scheduler Event Fns *******************/

/* This should be called with the scheduler lock */
int cos_sched_process_events(sched_evt_visitor_t fn, unsigned int proc_amnt)
{
	u8_t id, flags;
	u32_t v, v_new, cpu;
	u32_t *v_ptr;
	long ret;
	struct cos_sched_events *evt;
	struct cos_se_values *se;

	static u8_t cos_curr_evt = 0;

	if (!proc_amnt) {
		proc_amnt = ~(0UL);
	}

	/* 
	 * continue processessing until 1) the amount of items are
	 * processed as passed in, or 2) we have processed all events,
	 * i.e. the "next" field of the event is 0.
	 */
	while (proc_amnt > 0) {
		struct sched_thd *t;

		if (cos_curr_evt >= NUM_SCHED_EVTS) {
			return cos_curr_evt;
		}
		
		evt = &cos_sched_notifications.cos_events[cos_curr_evt];
		v_ptr = &COS_SCHED_EVT_VALS(evt);
		while (1) {
			v = *v_ptr;
			v_new = v;
//			print("event value all together is %x. %d%d", v, 0,0);
			se = (struct cos_se_values*)&v_new;
			id = se->next;
			flags = se->flags;
			se->next = 0;
			se->flags = 0;
			
			/* Lets try and avoid the compiler error in the fixme below */
			assert(!(v_new & 0xFFFF));
			ret = cos_cmpxchg(v_ptr, (long)v, (long)v_new);
			if (ret == (long)v_new) {
				break;
			}
		}
		while (1) {
			cpu = evt->cpu_consumption;
			ret = cos_cmpxchg(&evt->cpu_consumption, (long)cpu, 0);
			if (ret == 0) {
				break;
			}
		}

		if (cos_curr_evt) {
			t = sched_evt_to_thd(cos_curr_evt);
			if (t) {
				/* Call the visitor function */
				fn(t, flags, cpu);
			}
		}
		if (id) {
			cos_curr_evt = id;
		} else {
			break;
		}
		proc_amnt--;
	}

	return 0;
}

void cos_sched_set_evt_urgency(u8_t evt_id, u16_t urgency)
{
	struct cos_sched_events *evt;
	u32_t old, new;
	u32_t *ptr;
	//struct cos_se_values *se;

	assert(evt_id < NUM_SCHED_EVTS);

	evt = &cos_sched_notifications.cos_events[evt_id];
	ptr = &COS_SCHED_EVT_VALS(evt);

	/* Need to do this atomically with cmpxchg as next and flags
	 * are in the same word as the urgency.
	 */
	while (1) {
		old = *ptr;
		new = old;
		/* 
		 * FIXME: Seems as though GCC cannot handle this with
		 * -O2; not picking up the alias for some odd reason:
		 *
		 * se = (struct cos_se_values*)&new;
		 * se->urgency = urgency;
		 */
		new &= 0xFFFF;
		new |= urgency<<16;
		
		if (cos_cmpxchg(ptr, (long)old, (long)new) == (long)new) {
			break;
		}
	}

	return;
}

/* --- Thread Management Utiliities --- */

struct sched_thd *sched_thd_map[SCHED_NUM_THREADS];
struct sched_thd sched_thds[SCHED_NUM_THREADS]; 
struct sched_thd sched_grps[SCHED_NUM_THREADS];
struct sched_thd *sched_map_evt_thd[NUM_SCHED_EVTS];

void sched_init_thd(struct sched_thd *thd, unsigned short int thd_id, int flags)
{
	assert(!sched_thd_free(thd) && 
	       !sched_thd_grp(thd) && 
	       !sched_thd_member(thd));

	cos_memset(thd, 0, sizeof(struct sched_thd));
	INIT_LIST(thd, next, prev);
	INIT_LIST(thd, prio_next, prio_prev);
	thd->id = thd_id;
	thd->flags = flags;
}

short int sched_alloc_event(struct sched_thd *thd)
{
	int i;
	
	assert(thd->evt_id == 0);

	for (i = 1 ; i < NUM_SCHED_EVTS ; i++) {
		struct cos_sched_events *se;

		se = &cos_sched_notifications.cos_events[i];
		if (COS_SCHED_EVT_FLAGS(se) & COS_SCHED_EVT_FREE) {
			COS_SCHED_EVT_FLAGS(se) &= ~COS_SCHED_EVT_FREE;
			assert(sched_map_evt_thd[i] == NULL);
			/* add to evt thd -> thread map */
			sched_map_evt_thd[i] = thd;
			thd->evt_id = i;
			if (cos_sched_cntl(COS_SCHED_THD_EVT, thd->id, i)) {
				print("failed to allocate event. (%d%d%d)\n",1,1,1);
				COS_SCHED_EVT_FLAGS(se) |= COS_SCHED_EVT_FREE;
				return -1;
			}
			thd->event = i;

			return i;
		}
	}
	
	return -1;
}

void sched_ds_init(void) 
{
	int i;

	for (i = 0 ; i < SCHED_NUM_THREADS ; i++) {
		sched_thds[i].flags = THD_FREE;
		sched_grps[i].flags = THD_FREE;
	}
	for (i = 0 ; i < SCHED_NUM_THREADS ; i++) {
		sched_thd_map[i] = NULL;
	}
	for (i = 0 ; i < NUM_SCHED_EVTS ; i++) {
		struct cos_sched_events *se;

		se = &cos_sched_notifications.cos_events[i];
		if (i == 0) {
			COS_SCHED_EVT_FLAGS(se) = 0;
		} else {
			COS_SCHED_EVT_FLAGS(se) = COS_SCHED_EVT_FREE;
		}
		COS_SCHED_EVT_NEXT(se) = 0;
	}

	return;
}

struct sched_thd *sched_alloc_thd(unsigned short int thd_id)
{
	struct sched_thd *thd;

	assert(thd_id < SCHED_NUM_THREADS);

	thd = &sched_thds[thd_id];
	
	if (!(thd->flags & THD_FREE)) {
		return NULL;
	}

	thd->flags = 0;
	sched_init_thd(thd, thd_id, THD_READY);
	return thd;
}

struct sched_thd *sched_alloc_upcall_thd(unsigned short int thd_id)
{
	struct sched_thd *t = sched_alloc_thd(thd_id);

	if (!t) {
		return NULL;
	}

	t->flags = THD_UC_READY;
	return t; 
}

void sched_make_grp(struct sched_thd *thd, unsigned short int sched_thd)
{
	assert(!sched_thd_grp(thd) && 
	       !sched_thd_free(thd) && 
	       !sched_thd_member(thd));

	thd->flags |= THD_GRP;
	thd->id = sched_thd;
}

struct sched_thd *sched_alloc_grp(unsigned short int sched_thd)
{
	int i;
	struct sched_thd *thd;

	for (i = 0 ; i < SCHED_NUM_THREADS ; i++) {
		thd = &sched_grps[i];
		
		if (!(thd->flags & THD_FREE)) {
			continue;
		}
		
		thd->flags = 0;
		sched_init_thd(thd, sched_thd, THD_READY);
		sched_make_grp(thd, sched_thd);

		return thd;
	}

	return NULL;
} 

void sched_free_thd(struct sched_thd *thd)
{
	assert(!sched_thd_free(thd));

	thd->flags = THD_FREE;
}


void sched_add_grp(struct sched_thd *grp, struct sched_thd *thd)
{
	assert(sched_thd_grp(grp) &&
	       !sched_thd_free(grp) &&
	       !sched_thd_free(thd) && 
	       !sched_thd_grp(thd) && 
	       !sched_thd_member(thd));

	thd->flags |= THD_MEMBER;
	thd->group = grp;

	ADD_LIST(grp, thd, next, prev);
	grp->nthds++;
}

void sched_rem_grp(struct sched_thd *grp, struct sched_thd *thd)
{
	assert(sched_thd_grp(grp) &&
	       !sched_thd_free(grp) && 
	       !sched_thd_free(thd) && 
	       !sched_thd_grp(thd) && 
	       sched_thd_member(thd) &&
	       thd->group == grp);

	thd->group = NULL;
	thd->flags &= ~THD_MEMBER;

	REM_LIST(thd, next, prev);
	grp->nthds--;
}


