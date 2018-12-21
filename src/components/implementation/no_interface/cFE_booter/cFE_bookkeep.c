#include "cFE_bookkeep.h"

static struct cfe_bookkeep_info cfe_bkinfo[MAX_NUM_SPDS];

void
cfe_bookkeep_init(void)
{
	memset(cfe_bkinfo, 0, sizeof(struct cfe_bookkeep_info) * MAX_NUM_SPDS);
}

void
cfe_bookkeep_app_set(spdid_t s, unsigned int appid, struct sl_thd *initthd)
{
	assert(s && s < MAX_NUM_SPDS);

	cfe_bkinfo[s].appid = appid;
	cfe_bkinfo[s].inittid = sl_thd_thdid(initthd);
	cfe_bkinfo[s].thds[sl_thd_thdid(initthd)] = initthd;
	cfe_bkinfo[s].in_use = 1;
}

unsigned int
cfe_bookkeep_appid(spdid_t s)
{
	return cfe_bkinfo[s].appid;
}

struct sl_thd *
cfe_bookkeep_initthd(spdid_t s)
{
	return cfe_bkinfo[s].thds[cfe_bkinfo[s].inittid];
}

struct sl_thd *
cfe_bookkeep_thd(spdid_t s, thdid_t tid)
{
	return cfe_bkinfo[s].thds[tid];
}

void
cfe_bookkeep_thd_set(struct sl_thd *t)
{
	spdid_t s = cos_inv_token();

	if (s == 0) return;
	cfe_bkinfo[s].thds[sl_thd_thdid(t)] = t;
}

void
cfe_bookkeep_res_status_set(cfe_res_t r, unsigned int id, int flag)
{
	spdid_t s = cos_inv_token();

	/* only if created through a call from another comp, track them */
	if (s == 0) return;
	cfe_res(&cfe_bkinfo[s], r, id)->status |= CFE_RES_USING;
	cfe_res(&cfe_bkinfo[s], r, id)->status |= flag;
}

int
cfe_bookkeep_res_status(cfe_res_t r, unsigned int id)
{
	spdid_t s = cos_inv_token();

	if (s == 0) return 0;
	return cfe_res(&cfe_bkinfo[s], r, id)->status;
}

void
cfe_bookkeep_res_status_reset(cfe_res_t r, unsigned int id, int flag)
{
	spdid_t s = cos_inv_token();

	/* only if created through a call from another comp, track them */
	if (s == 0) return;
	cfe_res(&cfe_bkinfo[s], r, id)->status |= CFE_RES_USING;
	cfe_res(&cfe_bkinfo[s], r, id)->status &= ~flag;
}

void
cfe_bookkeep_res_name_set(cfe_res_t r, unsigned int id, char *name)
{
	spdid_t s = cos_inv_token();

	assert(name && strlen(name) < MAX_NAME);
	/* only if created through a call from another comp, track them */
	if (s == 0) return;
	cfe_res(&cfe_bkinfo[s], r, id)->status |= CFE_RES_USING;
	strcpy(cfe_res(&cfe_bkinfo[s], r, id)->name, name);
}

int
cfe_bookkeep_res_find(cfe_res_t r, char *name, unsigned int *id)
{
	spdid_t s = cos_inv_token();
	int i = 0, max = (r == CFE_RES_QUEUE ? OS_MAX_QUEUES : (r == CFE_RES_MUTEX ? OS_MAX_MUTEXES : (r == CFE_RES_COUNTSEM ? OS_MAX_COUNT_SEMAPHORES : (r == CFE_RES_BINSEM ? OS_MAX_BIN_SEMAPHORES : MAX_PIPES))));

	if (s == 0) return -EINVAL;
	assert(name && strlen(name) < MAX_NAME);
	for (i = 0; i < max; i++) {
		if ((cfe_res(&cfe_bkinfo[s], r, i)->status & CFE_RES_USING) &&
		    (strcmp(cfe_res(&cfe_bkinfo[s], r, i)->name, name) == 0)) {
			*id = i;
			break;
		}
	}

	if (i == max) return -EINVAL;

	return 0;
}

spdid_t
cfe_bookkeep_thdapp_get(thdid_t tid)
{
	int i = 0;
	spdid_t s = 0;

	for (i = 0; i < MAX_NUM_SPDS; i++) {
		int j;

		if (!cfe_bkinfo[i].in_use) continue;

		for (j = 0; j < MAX_NUM_THREADS; j++) {
			if (!cfe_bkinfo[i].thds[j]) continue;

			if (sl_thd_thdid(cfe_bkinfo[i].thds[j]) != tid) continue;

			s = i;
			break;
		}

		if (s) break;
	}

	return s;
}
