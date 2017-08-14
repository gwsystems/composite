/**
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Copyright 2014 by George Washington University.
 * Author: Qi Wang, interwq@gwu.edu, 2014
 */

#ifndef IPI_H
#define IPI_H

#include "chal.h"
#include "shared/cos_types.h"

/* We only use half of the cache_line so that the ring_size is power
 * of 2 (thus we can use mask and have no branch) */
#define IPI_RING_SIZE ((CACHE_LINE / 2) / sizeof(u32_t))
#define IPI_RING_MASK (IPI_RING_SIZE - 1);

struct IPI_producer_line {
	volatile u32_t tail;
	volatile u32_t ring[IPI_RING_SIZE];
	/* padding to prevent false sharing. */
	char _pad[CACHE_LINE - sizeof(u32_t) * (1 + IPI_RING_SIZE)];
} CACHE_ALIGNED __attribute__((packed));

struct IPI_consumer_line {
	volatile u32_t head;
	/* padding to prevent false sharing. */
	char _pad[CACHE_LINE - sizeof(u32_t)];
} CACHE_ALIGNED __attribute__((packed));

struct xcore_ring {
	struct IPI_producer_line sender;
	struct IPI_consumer_line receiver;
} CACHE_ALIGNED __attribute__((packed));

/* We make sure that, on the receiving side, the source rings are
 * lined up as we'll scan them upon receiving. This should benefit
 * the pre-fetcher. */
struct IPI_receiving_rings {
	struct xcore_ring IPI_source[NUM_CPU];
	/* Start core of each scan. Need to prevent starving. */
	u32_t start;
	/* padding to prevent false sharing. */
	char _pad[CACHE_LINE - sizeof(u32_t)];
} CACHE_ALIGNED __attribute__((packed));

struct IPI_receiving_rings IPI_dest[NUM_CPU];

static inline int
cos_ipi_ring_enqueue(u32_t dest, u32_t data)
{
	struct xcore_ring *ring  = &IPI_dest[dest].IPI_source[get_cpuid()];
	u32_t              tail  = ring->sender.tail;
	u32_t              delta = (tail + 1) & IPI_RING_MASK;

	if (unlikely(!data)) return -1;

	if (unlikely(delta == ring->receiver.head)) {
		/* printk("cos: IPI ring buffer full (from core %d to %d)\n", get_cpuid(), dest); */
		return -1;
	}

	ring->sender.ring[tail] = data;
	ring->sender.tail       = delta;
	/* Memory fence! */
	cos_mem_fence();

	/* printk("enqueue ring %x: dest %u, source %u, idx %u data %x\n", */
	/*        ring, dest, get_cpuid(), tail, data); */

	return 0;
}

/* returns spd_id + acap_id */
static inline u32_t
cos_ipi_ring_dequeue(struct xcore_ring *ring)
{
	u32_t data;
	/* printk("core %d ring : %x, sender %d, receiver %d\n", get_cpuid(), ring, ring->sender.tail,
	 * ring->receiver.head); */

	/* Do we need mem fence here? The sender had this already. */
	/* cos_mem_fence(); */

	if (ring->sender.tail == ring->receiver.head) return 0;

	data                = ring->sender.ring[ring->receiver.head];
	ring->receiver.head = (ring->receiver.head + 1) & IPI_RING_MASK;

	return data;
}

static inline void
handle_ipi_acap(int spd_id, int acap_id)
{
	struct spd *      spd;
	struct async_cap *acap;

	spd = spd_get_by_index(spd_id);
	if (unlikely(!spd)) {
		printk("cos: core %d received IPI but no valid data found (spd %d, acap %d)!\n", get_cpuid(), spd_id,
		       acap_id);
		return;
	}
	acap = &spd->acaps[acap_id];
	if (unlikely(!acap->allocated)) {
		printk("cos: core %d received IPI but no valid data found (spd %d, acap %d)!\n", get_cpuid(), spd_id,
		       acap_id);
		return;
	}

	if (!acap->upcall_thd) {
		/* No thread waiting yet. */
		acap->pending_upcall++;
		return;
	}
	/* printk("core %d receives ipi for acap %d, %x (thd %d) in spd %d\n",  */
	/*        get_cpuid(), acap_id, acap, acap->upcall_thd, spd_id); */

	/* Activate the associated thread. */
	chal_attempt_ainv(acap);
}

static inline void
process_ring(struct xcore_ring *ring)
{
	u32_t data;

	while ((data = cos_ipi_ring_dequeue(ring)) != 0) {
		handle_ipi_acap(data >> 16, data & 0xFFFF);
	}
}

static int
cos_send_ipi(int cpu, int spd_id, int acap_id)
{
	int                ret;
	u32_t              data = (spd_id << 16) | (acap_id);
	unsigned long long s    = 0, e;

	while (1) {
		ret = cos_ipi_ring_enqueue(cpu, data);
		if (likely(!ret)) break;

		/* avoid the rdtscll if buffer not full. */
		if (!s) rdtscll(s);
		rdtscll(e);
		if (e - s > (NUM_CPU * 1000 * 1000)) {
			printk("cos: WARNING: IPI ring buffer (Core %d) full - have been spinning for %llu cycles. No "
			       "IPI sent.\n",
			       cpu, e - s);
			return -1;
		}
	}
	chal_send_ipi(cpu);

	return 0;
}

#endif /* IPI_H */
