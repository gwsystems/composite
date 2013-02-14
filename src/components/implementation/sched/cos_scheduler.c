/*
 * Scheduler library that can be used by schedulers to manage their
 * data structures.
 *
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define COS_FMT_PRINT
#include <cos_scheduler.h>

/* Force the cos_asm_scheduler.S file to be linked with us */
extern int cos_force_sched_link;
int cos_use_force_sched_link(void)
{
	return cos_force_sched_link;
}

struct scheduler_per_core per_core_sched[NUM_CPU];

/* 
 * Use the visitor pattern here.  Pass in a function that will be
 * called for each entry.  This should be called with the scheduler
 * lock.
 */
int cos_sched_process_events(sched_evt_visitor_t fn, unsigned int proc_amnt)
{
	u8_t id, flags;
	u32_t cpu;
	long ret;
	struct cos_sched_events *evt;

	if (!proc_amnt) {
		proc_amnt = ~(0UL);
	}

	/* 
	 * continue processing until 1) the amount of items are
	 * processed as passed in, or 2) we have processed all events,
	 * i.e. the "next" field of the event is 0.
	 */
	while (proc_amnt > 0) {
		struct sched_thd *t;
		u32_t v, v_new, *v_ptr;

		if (per_core_sched[cos_cpuid()].cos_curr_evt >= NUM_SCHED_EVTS) {
			printc("cos curr evt %u", per_core_sched[cos_cpuid()].cos_curr_evt);
			assert(0);
			return -1;//return per_core_sched[cos_cpuid()].cos_curr_evt;
		}
		
		evt = &PERCPU_GET(cos_sched_notifications)->cos_events[per_core_sched[cos_cpuid()].cos_curr_evt];
		v_ptr = &COS_SCHED_EVT_VALS(evt);
		do {
			struct cos_se_values se;

			v_new = v = *v_ptr;
			memcpy(&se, &v_new, sizeof(u32_t));
			id = se.next;
			flags = se.flags;
			se.next = 0;
			se.flags = 0;
			memcpy(&v_new, &se, sizeof(u32_t));

			/* Lets try and avoid the compiler error due to union wierdness */
			assert(!(v_new & 0xFFFF));
			ret = cos_cmpxchg(v_ptr, (long)v, (long)v_new);
		} while (ret != (long)v_new);
		/* get and reset cpu consumption */
		do {
			cpu = evt->cpu_consumption;
			ret = cos_cmpxchg(&evt->cpu_consumption, (long)cpu, 0);
		} while (ret != 0);

		if ((cpu || flags) && per_core_sched[cos_cpuid()].cos_curr_evt) {
			t = sched_evt_to_thd(per_core_sched[cos_cpuid()].cos_curr_evt);
			if (t) {
				/* Call the visitor function */
				fn(t, flags, cpu);
			}
		}
		proc_amnt--;
		if (0 == id) break;
		per_core_sched[cos_cpuid()].cos_curr_evt = id;
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

	evt = &PERCPU_GET(cos_sched_notifications)->cos_events[evt_id];
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
		
		if (cos_cmpxchg(ptr, (long)old, (long)new) == (long)new) break;
	}

	return;
}

void sched_init_thd(struct sched_thd *thd, unsigned short int thd_id, int flags)
{
	assert(!sched_thd_free(thd) && 
	       !sched_thd_grp(thd) && 
	       !sched_thd_member(thd));

	cos_memset(thd, 0, sizeof(struct sched_thd));
	INIT_LIST(thd, next, prev);
	INIT_LIST(thd, prio_next, prio_prev);
	INIT_LIST(thd, sched_next, sched_prev);
	INIT_LIST(thd, cevt_next, cevt_prev);
	thd->id = thd_id;
	thd->flags = flags;
	thd->wake_cnt = 1;
}

int sched_share_event(struct sched_thd *n, struct sched_thd *old)
{
	int i;

	i = old->evt_id;
	assert(!(COS_SCHED_EVT_FLAGS(&PERCPU_GET(cos_sched_notifications)->cos_events[i]) & COS_SCHED_EVT_FREE));
	n->event = n->evt_id = i;
	if (cos_sched_cntl(COS_SCHED_THD_EVT, n->id, i)) return -1;

	return 0;
}

short int sched_alloc_event(struct sched_thd *thd)
{
	int i;
	
	assert(thd->evt_id == 0);

	for (i = 1 ; i < NUM_SCHED_EVTS ; i++) {
		struct cos_sched_events *se;

		se = &PERCPU_GET(cos_sched_notifications)->cos_events[i];
		if (COS_SCHED_EVT_FLAGS(se) & COS_SCHED_EVT_FREE) {
			COS_SCHED_EVT_FLAGS(se) &= ~COS_SCHED_EVT_FREE;
			assert(per_core_sched[cos_cpuid()].sched_map_evt_thd[i] == NULL);
			/* add to evt thd -> thread map */
			per_core_sched[cos_cpuid()].sched_map_evt_thd[i] = thd;
			thd->evt_id = i;
			if (cos_sched_cntl(COS_SCHED_THD_EVT, thd->id, i)) {
//				print("failed to allocate event. (%d%d%d)\n",1,1,1);
				COS_SCHED_EVT_FLAGS(se) |= COS_SCHED_EVT_FREE;
				return -1;
			}
			thd->event = i;

			return i;
		}
	}
	
	return -1;
}

int sched_rem_event(struct sched_thd *thd)
{
	int idx = thd->evt_id;
	struct cos_sched_events *se;
	assert(idx);

	se = &PERCPU_GET(cos_sched_notifications)->cos_events[idx];
	assert(!(COS_SCHED_EVT_FLAGS(se) & COS_SCHED_EVT_FREE));
	if (cos_sched_cntl(COS_SCHED_THD_EVT, thd->id, 0)) {
		return -1;
	}
	COS_SCHED_EVT_FLAGS(se) = COS_SCHED_EVT_FREE;
	per_core_sched[cos_cpuid()].sched_map_evt_thd[idx] = NULL;
	thd->event = 0;
	thd->evt_id = 0;

	return 0;
}

void sched_ds_init(void) 
{
	int i;

	for (i = 0 ; i < SCHED_NUM_THREADS ; i++) {
		per_core_sched[cos_cpuid()].sched_thds[i].flags = THD_FREE;
	}
	for (i = 0 ; i < SCHED_NUM_THREADS ; i++) {
		per_core_sched[cos_cpuid()].sched_thd_map[i] = NULL;
	}
	for (i = 0 ; i < NUM_SCHED_EVTS ; i++) {
		struct cos_sched_events *se;

		se = &PERCPU_GET(cos_sched_notifications)->cos_events[i];
		if (i == 0) {
			COS_SCHED_EVT_FLAGS(se) = 0;
		} else {
			COS_SCHED_EVT_FLAGS(se) = COS_SCHED_EVT_FREE;
		}
		COS_SCHED_EVT_NEXT(se) = 0;

		per_core_sched[cos_cpuid()].sched_map_evt_thd[i] = NULL;
	}

	sched_crit_sect_init();

	return;
}

struct sched_thd *sched_alloc_thd(unsigned short int thd_id)
{
	struct sched_thd *thd;

	assert(thd_id < SCHED_NUM_THREADS);

	thd = &per_core_sched[cos_cpuid()].sched_thds[thd_id];
	
	if (!(thd->flags & THD_FREE)) BUG();//return NULL;

	thd->flags = 0;
	sched_init_thd(thd, thd_id, THD_READY);
	return thd;
}

struct sched_thd *sched_alloc_upcall_thd(unsigned short int thd_id)
{
	struct sched_thd *t = sched_alloc_thd(thd_id);

	if (!t) return NULL;

	t->flags = THD_UC_READY;
	return t; 
}

void sched_free_thd(struct sched_thd *thd)
{
	assert(!sched_thd_free(thd));

	thd->flags = THD_FREE;
}

void sched_grp_make(struct sched_thd *thd, spdid_t csched)
{
	assert(!sched_thd_grp(thd) && !sched_thd_free(thd) && !sched_thd_member(thd));

	thd->flags |= THD_GRP;
	thd->cid = csched;
}

void sched_grp_add(struct sched_thd *grp, struct sched_thd *thd)
{
	assert(sched_thd_grp(grp) && !sched_thd_free(grp) && !sched_thd_free(thd) && 
	       !sched_thd_grp(thd) && !sched_thd_member(thd));

	thd->flags |= THD_MEMBER;
	thd->group = grp;

	ADD_LIST(grp, thd, next, prev);
}

void sched_grp_rem(struct sched_thd *thd)
{
	struct sched_thd *grp;

	grp = thd->group;
	assert(grp && sched_thd_grp(grp) && !sched_thd_free(grp) && !sched_thd_free(thd) && 
	       !sched_thd_grp(thd) && sched_thd_member(thd) && thd->group == grp);

	thd->group = NULL;
	thd->flags &= ~THD_MEMBER;

	REM_LIST(thd, next, prev);
	REM_LIST(thd, cevt_next, cevt_prev);
}

