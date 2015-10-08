/***
 * Copyright 2014-2015 by Gabriel Parmer.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Authors: Qi Wang, interwq@gmail.com, Gabriel Parmer, gparmer@gwu.edu, 2015
 *
 * History:
 * - Started as parsec.c and parsec.h by Qi.
 */

static inline int 
in_lib(struct quiescence_timing *timing) 
{
	/* this means not inside the lib. */
	if (timing->time_out > timing->time_in) return 0;

	return 1;
}

static inline void
timing_update_remote(struct percpu_info *curr, int remote_cpu, struct parsec *parsec)
{
	struct quiescence_timing *cpu_i;

	cpu_i = &(parsec->timing_info[remote_cpu].timing);

	curr->timing_others[remote_cpu].time_in  = cpu_i->time_in;
	curr->timing_others[remote_cpu].time_out = cpu_i->time_out;

	/* 
	 * We are reading remote cachelines possibly, so this time
	 * stamp reading cost is fine.
	 */
	curr->timing_others[remote_cpu].time_updated = ps_tsc();

	/* If remote core has information that can help, use it. */
	if (curr->timing.last_known_quiescence < cpu_i->last_known_quiescence) {
		curr->timing.last_known_quiescence = cpu_i->last_known_quiescence;
	}

	cos_mem_fence();

	return;
}

int 
parsec_sync_quiescence(ps_tsc_t orig_timestamp, const int waiting, struct parsec *parsec)
{
	int inlib_curr, quie_cpu, curr_cpu, first_try, i, done_i;
	ps_tsc_t min_known_quie;
	ps_tsc_t in, out, update;
	struct percpu_info *cpuinfo;
	struct quiescence_timing *timing_local;
	ps_tsc_t time_check;

	time_check = orig_timestamp;

	curr_cpu  = get_cpu();

	cpuinfo = &(parsec->timing_info[curr_cpu]);
	timing_local = &cpuinfo->timing;

	inlib_curr = in_lib(timing_local);
	/* 
	 * ensure quiescence on the current core: either time_in >
	 * time_check, or we are not in the lib right now. 
	 */
	if (unlikely((time_check > timing_local->time_in) 
		     && inlib_curr)) {
		/* printf(">>>>>>>>>> QUIESCENCE wait error %llu %llu!\n",  */
		/*        time_check, timing_local->time_in); */
		return -EQUIESCENCE;
	}

	min_known_quie = (unsigned long long)(-1);
	for (i = 1; i < PS_NUMCORES; i++) {
		quie_cpu = (curr_cpu + i) % PS_NUMCORES;
		assert(quie_cpu != curr_cpu);

		first_try = 1;
		done_i = 0;
re_check:
		if (time_check < timing_local->last_known_quiescence) 
			break;

		in     = cpuinfo->timing_others[quie_cpu].time_in;
		out    = cpuinfo->timing_others[quie_cpu].time_out;
		update = cpuinfo->timing_others[quie_cpu].time_updated;

		if ((time_check < in) || 
		    ((time_check < update) && (in < out))) {
			done_i = 1;
		}
		
		if (done_i) {
			/* assertion: update >= in */
			if (in < out) {
				if (min_known_quie > update) min_known_quie = update;
			} else {
				if (min_known_quie > in) min_known_quie = in;
			}
			continue;
		}

		/* 
		 * If no waiting allowed, then read at most one remote
		 * cacheline per core. 
		 */
		if (first_try) {
			first_try = 0;
		} else {
			if (!waiting) return -1;
		}

		timing_update_remote(cpuinfo, quie_cpu, parsec);

		goto re_check;
	}

	/* 
	 * This would be on the fast path of the single core/thd
	 * case. Optimize a little bit.
	 */
#if PS_NUMCORES > 1
	if (i == PS_NUMCORES) {
		if (inlib_curr && (min_known_quie > timing_local->time_in))
			min_known_quie = timing_local->time_in;

		assert(min_known_quie < (unsigned long long)(-1));
		/* 
		 * This implies we went through all cores. Thus the
		 * min_known_quie can be used to determine global
		 * quiescence. 
		 */
		if (timing_local->last_known_quiescence < min_known_quie)
			timing_local->last_known_quiescence = min_known_quie;
		cos_mem_fence();
	}
#endif

	return 0;
}

/* force waiting for quiescence */
int 
parsec_quiescence_wait(ps_tsc_t orig_timestamp, struct parsec *p)
{
	/* waiting for quiescence if needed. */
	return parsec_sync_quiescence(orig_timestamp, 1, p);
}

int 
parsec_quiescence_check(ps_tsc_t orig_timestamp, struct parsec *p)
{
	/* non-waiting */
	return parsec_sync_quiescence(orig_timestamp, 0, p);
}

void 
parsec_read_lock(struct parsec *parsec) 
{
	int curr_cpu;
	ps_tsc_t curr_time;
	struct quiescence_timing *timing;
	
	curr_cpu  = ps_coreid();
	curr_time = ps_tsc();

	timing = &(parsec->timing_info[curr_cpu].timing);
	timing->time_in = curr_time;
	
	/* 
	 * The following is needed when we have coarse granularity
	 * time-stamps (i.e. non-cycle granularity, which means we
	 * could have same time-stamp for different events).
	 */
	timing->time_out = curr_time - 1;

	cos_mem_fence();
	
	return;
}

void 
parsec_read_unlock(struct parsec *parsec) 
{
	int curr_cpu;
	struct quiescence_timing *timing;
	
	curr_cpu = get_cpu();
	timing = &(parsec->timing_info[curr_cpu].timing);

	/* barrier, then write time stamp. */

	/* 
	 * Here we don't require a full memory barrier on x86 -- only
	 * a compiler barrier is enough.
	 */
	cmm_barrier();

	timing->time_out = timing->time_in + 1;
	
	return;
}

void 
ps_enter(struct parsec *parsec) 
{ parsec_read_lock(parsec); }

void 
ps_exit(struct parsec *parsec) 
{ parsec_read_unlock(parsec); }

/* not used for now */
static void 
parsec_quiesce(struct parsec *p)
{
	ps_tsc_t t = get_time();
	
	lib_enter(p);
	parsec_quiescence_wait(t, p);
	lib_exit(p);
}
