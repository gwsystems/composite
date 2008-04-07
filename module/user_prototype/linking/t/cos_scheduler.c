#include <cos_scheduler.h>

struct sched_thd *thd_map[SCHED_NUM_THREADS];
struct sched_thd thds[SCHED_NUM_EXECUTABLES]; 

/* --- Thread Management Utiliities --- */

void sched_init_thd(struct sched_thd *thd, unsigned short int thd_id)
{
	assert(!sched_thd_free(thd) && 
	       !sched_thd_grp(thd) && 
	       !sched_thd_member(thd));

	cos_memset(thd, 0, sizeof(struct sched_thd));
	INIT_LIST(thd, next, prev);
	INIT_LIST(thd, prio_next, prio_prev);
	thd->id = thd_id;
}

void sched_ds_init(void) 
{
	int i;

	for (i = 0 ; i < SCHED_NUM_EXECUTABLES ; i++) {
		thds[i].flags = THD_FREE;
	}
	for (i = 0 ; i < SCHED_NUM_THREADS ; i++) {
		thd_map[i] = NULL;
	}

	return;
}

struct sched_thd *sched_alloc_thd(unsigned short int thd_id)
{
	int i;

	for (i = 0 ; i < SCHED_NUM_EXECUTABLES ; i++) {
		struct sched_thd *thd = &thds[i];

		if (thd->flags & THD_FREE) {
			thd->flags = THD_READY;
			sched_init_thd(thd, thd_id);
			return thd;
		}
	}
	
	return NULL;
}

void sched_free_thd(struct sched_thd *thd)
{
	assert(!sched_thd_free(thd));

	thd->flags = THD_FREE;
}

void sched_make_grp(struct sched_thd *thd, unsigned short int sched_thd)
{
	assert(!sched_thd_grp(thd) && 
	       !sched_thd_free(thd) && 
	       !sched_thd_member(thd));

	thd->flags |= THD_GRP;
	thd->id = sched_thd;
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


