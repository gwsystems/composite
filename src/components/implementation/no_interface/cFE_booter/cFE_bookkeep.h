#ifndef CFE_BOOKKEEP_H
#define CFE_BOOKKEEP_H

#include <cos_types.h>
#include <sl.h>

#include "gen/osapi.h"

#define MAX_PIPES 256
#define MAX_NAME  64

typedef enum {
	CFE_RES_QUEUE = 0,
	CFE_RES_MUTEX,
	CFE_RES_COUNTSEM,
	CFE_RES_BINSEM,
	CFE_RES_SBPIPE,
} cfe_res_t;

enum {
	CFE_RES_USING = 1,
	CFE_RES_LOCKED = 1<<1,
};

struct cfe_resource_status {
	unsigned int status;
	char name[MAX_NAME];
};

struct cfe_bookkeep_info {
	struct sl_thd *thds[MAX_NUM_THREADS];
	unsigned int appid;
	thdid_t      inittid;
	int          in_use;

	struct cfe_resource_status queues[OS_MAX_QUEUES];
	struct cfe_resource_status mutexes[OS_MAX_MUTEXES];
	struct cfe_resource_status countsem[OS_MAX_COUNT_SEMAPHORES];
	struct cfe_resource_status binsem[OS_MAX_BIN_SEMAPHORES];
	struct cfe_resource_status sbpipe[MAX_PIPES];
	/* TODO: open files */
	/* TIMERS are handled in the interface layer!, so there is no way to! */
};

static inline struct cfe_resource_status *
cfe_res(struct cfe_bookkeep_info *bki, cfe_res_t r, unsigned int id)
{
	switch(r) {
	case CFE_RES_QUEUE: return &bki->queues[id];
	case CFE_RES_MUTEX: return &bki->mutexes[id];
	case CFE_RES_BINSEM: return &bki->binsem[id];
	case CFE_RES_COUNTSEM: return &bki->countsem[id];
	case CFE_RES_SBPIPE: return &bki->sbpipe[id];
	}

	return NULL;
}

void cfe_bookkeep_init(void);
void cfe_bookkeep_app_set(spdid_t s, unsigned int appid, struct sl_thd *t);
void cfe_bookkeep_thd_set(struct sl_thd *t);
unsigned int cfe_bookkeep_appid(spdid_t s);
struct sl_thd *cfe_bookkeep_initthd(spdid_t s);
struct sl_thd *cfe_bookkeep_thd(spdid_t s, thdid_t tid);

void cfe_bookkeep_res_status_set(cfe_res_t r, unsigned int id, int flag);
void cfe_bookkeep_res_status_reset(cfe_res_t r, unsigned int id, int flag);
int cfe_bookkeep_res_status(cfe_res_t r, unsigned int id);
void cfe_bookkeep_res_name_set(cfe_res_t r, unsigned int id, char *name);
int cfe_bookkeep_res_find(cfe_res_t r, char *name, unsigned int *id);

spdid_t cfe_bookkeep_thdapp_get(thdid_t);

#endif /* CFE_BOOKKEEP_H */
