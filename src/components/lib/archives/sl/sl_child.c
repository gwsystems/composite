/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <sl.h>
#include <sl_child.h>

static struct ck_ring *child_ring[NUM_CPU] = { NULL };
static struct sl_child_notification *child_ringbuf[NUM_CPU] = { NULL };

extern cbuf_t  sl_shm_alloc(vaddr_t *addr);
extern vaddr_t sl_shm_map(cbuf_t id);

cbuf_t
sl_parent_notif_alloc(struct sl_thd *childthd)
{
	vaddr_t shmaddr = 0;
	cbuf_t id = 0;
	struct ck_ring *cring = NULL;
	struct sl_child_notification *crbuf = NULL;

	assert(childthd && (childthd->properties & SL_THD_PROPERTY_SEND));

	sl_cs_enter();

	id = sl_shm_alloc(&shmaddr);
	if (!id) goto done;

	assert(shmaddr);
	cring = sl_child_ring(shmaddr);
	crbuf = sl_child_notif_buffer(shmaddr);

	ck_ring_init(cring, SL_CHILD_RBUF_SZ);
	childthd->shmid      = id;
	childthd->ch_ringbuf = crbuf;
	childthd->ch_ring    = cring;

done:
	sl_cs_exit();

	return id;
}

int
sl_parent_notif_enqueue(struct sl_thd *thd, struct sl_child_notification *notif)
{
	assert(thd && notif);
	assert(thd->properties & SL_THD_PROPERTY_SEND);

	if (!thd->ch_ring) return -1;
	assert(thd->ch_ringbuf);

	if (ck_ring_enqueue_spsc_child(thd->ch_ring, thd->ch_ringbuf, notif) == false) return -1;
	if (cos_asnd(sl_thd_asndcap(thd), 0)) return -1;

	return 0;
}

/* there is only 1 parent per scheduler per cpu */
int
sl_child_notif_map(cbuf_t id)
{
	vaddr_t shmaddr = 0;

	assert(id);

	sl_cs_enter();
	shmaddr = sl_shm_map(id);
	if (!shmaddr) {
		sl_cs_exit();
		return -1;
	}
	assert(!child_ring[cos_cpuid()]);

	child_ring[cos_cpuid()]    = sl_child_ring(shmaddr);
	child_ringbuf[cos_cpuid()] = sl_child_notif_buffer(shmaddr);
	sl_cs_exit();

	return 0;
}

int
sl_child_notif_dequeue(struct sl_child_notification *notif)
{
	struct ck_ring *cring = child_ring[cos_cpuid()];
	struct sl_child_notification *crbuf = child_ringbuf[cos_cpuid()];

	assert(notif);
	if (!cring || !crbuf) return 0;

	if (ck_ring_dequeue_spsc_child(cring, crbuf, notif) == true) return 1;

	return 0;
}

int
sl_child_notif_empty(void)
{
	struct ck_ring *cring = child_ring[cos_cpuid()];

	if (!cring) return 1;

	return (!ck_ring_size(cring));
}

int
sl_parent_notif_block_no_cs(struct sl_thd *child, struct sl_thd *thd)
{
	struct sl_child_notification notif;

	notif.type = SL_CHILD_THD_BLOCK;
	notif.tid  = sl_thd_thdid(thd);

	return sl_parent_notif_enqueue(child, &notif);
}

int
sl_parent_notif_wakeup_no_cs(struct sl_thd *child, struct sl_thd *thd)
{
	struct sl_child_notification notif;

	notif.type = SL_CHILD_THD_WAKEUP;
	notif.tid  = sl_thd_thdid(thd);

	return sl_parent_notif_enqueue(child, &notif);
}
