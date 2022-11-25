#include <sl.h>
#include <sl_blkpt.h>
#include <stacklist.h>

#define NBLKPTS 64
struct blkpt_mem {
	sched_blkpt_id_t      id;
	sched_blkpt_epoch_t   epoch;
	struct stacklist_head blocked;
};
static struct blkpt_mem __blkpts[NBLKPTS];
static int __blkpt_offset = 1;

#define BLKPT_EPOCH_BLKED_BITS ((sizeof(sched_blkpt_epoch_t) * 8)
#define BLKPT_EPOCH_DIFF       (BLKPT_EPOCH_BLKED_BITS - 2)/2)

/*
 * Is cmp > e? This is more complicated than it seems it should be
 * only because of wrap-around. We have to consider the case that we
 * have, and that we haven't wrapped around.
 */
static int
blkpt_epoch_is_higher(sched_blkpt_epoch_t e, sched_blkpt_epoch_t cmp)
{
	return (e > cmp && (e - cmp) > BLKPT_EPOCH_DIFF) || (e < cmp && (cmp - e) < BLKPT_EPOCH_DIFF);
}

static struct blkpt_mem *
blkpt_get(sched_blkpt_id_t id)
{
	if (id - 1 == NBLKPTS) return NULL;

	return &__blkpts[id-1];
}

sched_blkpt_id_t
sl_blkpt_alloc(void)
{
	sched_blkpt_id_t id;
	struct blkpt_mem *m;
	sched_blkpt_id_t ret = SCHED_BLKPT_NULL;

	sl_cs_enter();

	id = (sched_blkpt_id_t)__blkpt_offset;
	m  = blkpt_get(id);
	if (!m) ERR_THROW(SCHED_BLKPT_NULL, unlock);

	m->id    = id;
	ret      = id;
	m->epoch = 0;
	stacklist_init(&m->blocked);
	__blkpt_offset++;
unlock:
	sl_cs_exit();

	return ret;
}

int
sl_blkpt_free(sched_blkpt_id_t id)
{
	/* alloc only for now */
	return 0;
}

int
sl_blkpt_trigger(sched_blkpt_id_t blkpt, sched_blkpt_epoch_t epoch, int single)
{
	struct sl_thd *t;
	struct blkpt_mem *m;
	int ret = 0;

	sl_cs_enter();

	m = blkpt_get(blkpt);
	if (!m) ERR_THROW(-1, unlock);

	/* is the new epoch more recent than the existing? */
	if (!blkpt_epoch_is_higher(m->epoch, epoch)) ERR_THROW(0, unlock);

	m->epoch = epoch;
	while ((t = stacklist_dequeue(&m->blocked)) != 0) {
		sl_thd_wakeup_no_cs(t); /* ignore retval: process next thread */
	}
	/* most likely we switch to a woken thread here */
	sl_cs_exit_schedule();

	return 0;
unlock:
	sl_cs_exit();

	return ret;
}

int
sl_blkpt_block(sched_blkpt_id_t blkpt, sched_blkpt_epoch_t epoch, thdid_t dependency)
{
	struct blkpt_mem *m;
	struct sl_thd    *t;
	struct stacklist  sl; 	/* The stack-based structure we'll use to track ourself */
	int ret = 0;

	sl_cs_enter();

	m = blkpt_get(blkpt);
	if (!m) ERR_THROW(-1, unlock);

	/* Outdated event? don't block! */
	if (blkpt_epoch_is_higher(m->epoch, epoch)) ERR_THROW(0, unlock);

	/* Block! */
	t = sl_thd_curr();
	stacklist_add(&m->blocked, &sl, t);
	if (sl_thd_block_no_cs(t, SL_THD_BLOCKED, 0)) ERR_THROW(-1, unlock);

	sl_cs_exit_schedule();
	assert(stacklist_is_removed(&sl)); /* we cannot still be on the list */

	return 0;
unlock:
	sl_cs_exit();

	return ret;
}
