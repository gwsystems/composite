/**
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Copyright 2014 by George Washington University.
 * Author: Qi Wang, interwq@gwu.edu, 2014
 */

#ifndef IPI_CAP_H
#define IPI_CAP_H

#include "chal.h"
#include "inv.h" /* need access to cap_asnd/arcv*/
#include "shared/cos_types.h"

/*
 * Ring size should be power of 2
 * We have N*N rings (N= # of cpus).
 */
#define IPI_RING_SIZE (16)
#define IPI_RING_MASK (IPI_RING_SIZE - 1);

struct ipi_cap_data {
	capid_t          arcv_capid;
	capid_t          arcv_epoch;
	struct comp_info comp_info;
};

struct xcore_ring {
	volatile u32_t      sender;
	char                _pad[CACHE_LINE - sizeof(u32_t)];
	volatile u32_t      receiver;
	char                __pad[CACHE_LINE - sizeof(u32_t)];
	struct ipi_cap_data ring[IPI_RING_SIZE];
	char                ___pad[CACHE_LINE - (sizeof(struct ipi_cap_data) * IPI_RING_SIZE) % CACHE_LINE];
} CACHE_ALIGNED __attribute__((packed));

/*
 * We make sure that, on the receiving side, the source rings are
 * lined up as we'll scan them upon receiving. This should benefit
 * the pre-fetcher.
 */
struct IPI_receiving_rings {
	struct xcore_ring IPI_source[NUM_CPU];
	/* Start core of each scan. Need to prevent starving. */
	u32_t start;
	/* padding to prevent false sharing. */
	char _pad[CACHE_LINE - sizeof(u32_t)];
} CACHE_ALIGNED __attribute__((packed));

struct IPI_receiving_rings IPI_cap_dest[NUM_CPU] CACHE_ALIGNED;

static inline u32_t
cos_ipi_ring_dequeue(struct xcore_ring *ring, struct ipi_cap_data *ret)
{
	/* printk("core %d ring : %x, sender %d, receiver %d\n", get_cpuid(), ring, ring->sender.tail,
	 * ring->receiver.head); */

	if (ring->sender == ring->receiver) return 0;
	memcpy(ret, &ring->ring[ring->receiver], sizeof(struct ipi_cap_data));

	ring->receiver = (ring->receiver + 1) & IPI_RING_MASK;

	cos_mem_fence();

	return 1;
}

static inline struct cap_arcv *
cos_ipi_arcv_get(struct ipi_cap_data *data)
{
	struct comp_info *ci = &data->comp_info;
	struct cap_arcv  *arcv;
	/* FIXME: check epoch and liveness! */

	assert(ci->captbl);
	arcv = (struct cap_arcv *)captbl_lkup(ci->captbl, data->arcv_capid);
	if (unlikely(arcv->h.type != CAP_ARCV)) {
		printk("cos: IPI handling received invalid arcv cap %d\n", (int)data->arcv_capid);
		return 0;
	}

	return arcv;
}

static inline void
process_ring(struct xcore_ring *ring)
{
	return;
}

static inline int
cos_ipi_ring_enqueue(u32_t dest, struct cap_asnd *asnd)
{
	struct xcore_ring *  ring;
	u32_t                tail;
	u32_t                delta;
	struct ipi_cap_data *data;

	if (unlikely(dest >= NUM_CPU)) return -EINVAL;

	ring = &IPI_cap_dest[dest].IPI_source[get_cpuid()];
	tail = ring->sender;

	delta = (tail + 1) & IPI_RING_MASK;
	data  = &ring->ring[tail];
	if (unlikely(delta == ring->receiver)) return -EBUSY;

	data->arcv_capid = asnd->arcv_capid;
	data->arcv_epoch = asnd->arcv_epoch;
	memcpy(&data->comp_info, &asnd->comp_info, sizeof(struct comp_info));

	ring->sender = delta;

	cos_mem_fence();

	return 0;
}

static int
cos_cap_send_ipi(int cpu, struct cap_asnd *asnd)
{
	int ret;

	ret = cos_ipi_ring_enqueue(cpu, asnd);
	if (unlikely(ret)) return ret;

	chal_send_ipi(cpu);

	return 0;
}

#endif /* IPI_CAP_H */
