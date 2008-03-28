#include "cos_scheduler.h"

struct sched_thread *thd_map[SCHED_NUM_THREADS];
struct sched_thread thds[SCHED_NUM_EXECUTABLES]; 

/* --- Thread Management Utiliities --- */

void sched_init_thd_array(void) 
{
	int i;

	for (i = 0 ; i < SCHED_NUM_EXECUTABLES ; i++) {
		thds[i]->status = THD_FREE;
	}

	return;
}

void sched_init_thd(struct sched_thd *thd)
{
	memset(thd, 0, sizeof(struct sched_thd));
	thd->next = thd->prev = thd;
}

struct sched_thd *sched_alloc_thd(void)
{
	int i;

	for (i = 0 ; i < SCHED_NUM_EXECUTABLES ; i++) {
		struct sched_thd *thd = &thds[i];

		if (thd->flags & THD_FREE) {
			thd->flags &= ~THD_FREE;
			return thd;
		}
	}
	
	return NULL;
}

void sched_free_thd(struct sched_thd *thd)
{
	assert(thd->flags & THD_FREE == 0);

	thd->flags = THD_FREE;
}

void sched_make_grp(struct sched_thd *thd, unsigned short int sched_thd)
{
	assert(thd->flags & THD_GRP == 0 && 
	       thd->flags & THD_FREE == 0 && 
	       thd->flags & THD_MEMBER == 0);

	thd->flags |= THD_GRP;
	thd->thd_id = sched_thd;
}

void sched_init_thd(struct sched_thd *thd, unsigned short int thd_id)
{
	assert(thd->flags & THD_FREE == 0 && 
	       thd->flags & THD_GRP == 0 && 
	       thd->flags & THD_MEMBER == 0);

	thd->thd_id = thd_id;
}

void sched_add_grp(struct sched_thd *grp, struct sched_thd *thd)
{
	struct sched_thd *tmp;

	assert(grp->flags & THD_GRP != 0 && 
	       grp->flags & THD_FREE == 0 && 
	       thd->flags & THD_FREE == 0 && 
	       thd->flags & THD_GRP == 0 && 
	       thd->flags & THD_MEMBER == 0);

	thd->flags |= THD_MEMBER;
	thd->group = grp;
	if (NULL == grp->threads) {
		grp->threads = thd;
	} else {
		l_add_sched_thd(grp->threads, thd);
	}
	grp->nthds++;
}

void sched_rem_grp(struct sched_thd *grp, struct sched_thd *thd)
{
	assert(grp->flags & THD_GRP && 
	       grp->flags & THD_FREE == 0 && 
	       thd->flags & THD_FREE == 0 && 
	       thd->flags & THD_GRP == 0 && 
	       thd->flags & THD_MEMBER == 0 &&
	       thd->group == grp);

	thd->group = NULL;
	thd->flags &= ~THD_MEMBER;
	if (nthds == 1) {
		grp->threads = NULL;
	} else {
		l_rem_sched_thd(thd);
	}
}

int sched_is_grp(struct sched_thd *thd)
{
	assert(thd->flags & THD_FREE == 0);

	if (thd->flags & THD_GRP) {
		assert(thd->flags & THD_MEMBER == 0);
		
		return 1;
	}
	assert(thd->flags & THD_GRP == 0);

	return 0;
}

struct sched_thd *sched_get_members(struct sched_thd *grp)
{
	assert(grp->flags & THD_FREE == 0 && 
	       grp->flags & THD_GRP);

	return grp->threads;
}

struct sched_thd *sched_get_grp(struct sched_thd *thd)
{
	if (sched_is_grp(thd)) {
		return NULL;
	}
	return thd->group;
}

/* --- RunQueue Management --- */

