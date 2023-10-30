/*-
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#include <assert.h>
#include <cos_types.h>
#include "virtio_net_io.h"
#include "vpci.h"
#include "virtio_ring.h"

/* TODO: remove this warning flag when virtio-net is done */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

static struct virtio_net_io_reg virtio_net_regs;
static struct virtio_queue virtio_queues[2];
struct virtio_vq_info virtio_net_vqs[VIRTIO_NET_MAXQ - 1];

static unsigned char icmp_reply[] = {
	/* TODO: remove icmp reply and process nic input from nic component */
};

static inline int
vq_ring_ready(struct virtio_vq_info *vq)
{
	return 1;
}

static inline int 
vq_has_descs(struct virtio_vq_info *vq)
{
	int ret = 0;
	if (vq_ring_ready(vq) && vq->last_avail != vq->avail->idx) {
		if ((u16_t)((unsigned int)vq->avail->idx - vq->last_avail) > vq->qsize)
			printc ("no valid descriptor\n");
		else
			ret = 1;
	}
	return ret;

}

void *
paddr_guest2host(uintptr_t gaddr, struct vmrt_vm_comp *vm)
{
	void *va = GPA2HVA(gaddr, vm);
	return va;
}

#define roundup2(x, y)  (((x)+((y)-1))&(~((y)-1)))
#define mb()    ({ asm volatile("mfence" ::: "memory"); (void)0; })

void
vq_endchains(struct vmrt_vm_vcpu *vcpu, struct virtio_vq_info *vq, int used_all_avail)
{
	u16_t event_idx, new_idx, old_idx;
	int intr;

	if (!vq || !vq->used)
		return;
	
	/* TODO: 57 is virtio-net interrupt, should read it from somewhere else more reliable */
	lapic_intr_inject(vcpu, 57, 0);
}

static inline struct iovec *
rx_iov_trim(struct iovec *iov, int *niov, size_t tlen)
{
	struct iovec *riov;

	/* XXX short-cut: assume first segment is >= tlen */
	if (iov[0].iov_len < tlen) {
		printc("vtnet: rx_iov_trim: iov_len=%lu, tlen=%lu\n", iov[0].iov_len, tlen);
		return NULL;
	}

	iov[0].iov_len -= tlen;
	if (iov[0].iov_len == 0) {
		if (*niov <= 1) {
			printc("vtnet: rx_iov_trim: *niov=%d\n", *niov);
			return NULL;
		}
		*niov -= 1;
		riov = &iov[1];
	} else {
		iov[0].iov_base = (void *)((uintptr_t)iov[0].iov_base + tlen);
		riov = &iov[0];
	}

	return riov;
}

static void
virtio_vq_init(struct vmrt_vm_vcpu *vcpu, int nr_queue, u32_t pfn)
{
	struct virtio_vq_info *vq;
	u64_t phys;
	size_t size;
	char *vb;

	vq = &virtio_net_vqs[nr_queue];
	vq->pfn = pfn;
	phys = (u64_t)pfn << VRING_PAGE_BITS;
	size = vring_size(vq->qsize, VIRTIO_PCI_VRING_ALIGN);
	vb = paddr_guest2host(phys, vcpu->vm);
	if (!vb)
		goto error;

	/* First page(s) are descriptors... */
	vq->desc = (struct vring_desc *)vb;
	vb += vq->qsize * sizeof(struct vring_desc);

	/* ... immediately followed by "avail" ring (entirely u16_t's) */
	vq->avail = (struct vring_avail *)vb;
	vb += (2 + vq->qsize + 1) * sizeof(u16_t);

	/* Then it's rounded up to the next page... */
	vb = (char *)roundup2((uintptr_t)vb, VIRTIO_PCI_VRING_ALIGN);

	/* ... and the last page(s) are the used ring. */
	vq->used = (struct vring_used *)vb;

	/* Start at 0 when we use it. */
	vq->last_avail = 0;
	vq->save_used = 0;

	/* Mark queue as allocated after initialization is complete. */
	mb();
	vq->flags = VQ_ALLOC;

	printc("%s: vq enable done\n", __func__);
	return;

error:
	vq->flags = 0;
	printc("%s: vq enable failed\n", __func__);
}

static inline int
_vq_record(int i, volatile struct vring_desc *vd,
	   struct iovec *iov, int n_iov, u16_t *flags, struct vmrt_vm_vcpu *vcpu) {

	void *host_addr;

	if (i >= n_iov)
		return -1;
	host_addr = paddr_guest2host(vd->addr, vcpu->vm);
	if (!host_addr)
		return -1;
	iov[i].iov_base = host_addr;
	iov[i].iov_len = vd->len;
	if (flags != NULL)
		flags[i] = vd->flags;
	return 0;
}

static void
dump_packet(char *p, u16_t sz)
{
	for (int i = 0; i < sz; i++) {
		if (i > 0 && (i % 8 == 0)) printc("\n");
		u8_t c = p[i];
		printc("%02x ", c);
	}
	printc("\n");
}

int
vq_getchain(struct vmrt_vm_vcpu *vcpu, struct virtio_vq_info *vq, u16_t *pidx,
	    struct iovec *iov, int n_iov, u16_t *flags)
{
	int i;
	unsigned int ndesc, n_indir;
	unsigned int idx, next;

	volatile struct vring_desc *vdir, *vindir, *vp;
	const char *name = "testnic"; 
	/*
	 * Note: it's the responsibility of the guest not to
	 * update vq->avail->idx until all of the descriptors
	 * the guest has written are valid (including all their
	 * next fields and vd_flags).
	 *
	 * Compute (last_avail - idx) in integers mod 2**16.  This is
	 * the number of descriptors the device has made available
	 * since the last time we updated vq->last_avail.
	 *
	 * We just need to do the subtraction as an unsigned int,
	 * then trim off excess bits.
	 */
	idx = vq->last_avail;
	ndesc = (u16_t)((unsigned int)vq->avail->idx - idx);
	if (ndesc == 0)
		return 0;
	if (ndesc > vq->qsize) {
		/* XXX need better way to diagnose issues */
		printc("%s: ndesc (%u) out of range, driver confused?\r\n",
		    name, (unsigned int)ndesc);
		return -1;
	}

	/*
	 * Now count/parse "involved" descriptors starting from
	 * the head of the chain.
	 *
	 * To prevent loops, we could be more complicated and
	 * check whether we're re-visiting a previously visited
	 * index, but we just abort if the count gets excessive.
	 */
	*pidx = next = vq->avail->ring[idx & (vq->qsize - 1)];
	vq->last_avail++;
	for (i = 0; i < VQ_MAX_DESCRIPTORS; next = vdir->next) {
		if (next >= vq->qsize) {
			printc("tx: descriptor index %u out of range, "
			    "driver confused?\r\n",
			     next);
			return -1;
		}
		vdir = &vq->desc[next];
		if ((vdir->flags & VRING_DESC_F_INDIRECT) == 0) {
			if (_vq_record(i, vdir, iov, n_iov, flags, vcpu)) {
				printc("%s: mapping to host failed\r\n", name);
				return -1;
			}
			i++;
		} else if ((virtio_net_regs.header.dev_features &
		    (1 << VIRTIO_RING_F_INDIRECT_DESC)) == 0) {
			printc("%s: descriptor has forbidden INDIRECT flag, "
			    "driver confused?\r\n",
			    name);
			return -1;
		} else {
			n_indir = vdir->len / 16;
			if ((vdir->len & 0xf) || n_indir == 0) {
				printc("%s: invalid indir len 0x%x, "
				    "driver confused?\r\n",
				    name, (unsigned int)vdir->len);
				return -1;
			}
			vindir = paddr_guest2host(
			    vdir->addr, vcpu->vm);

			if (!vindir) {
				printc("%s cannot get host memory\r\n", name);
				return -1;
			}
			/*
			 * Indirects start at the 0th, then follow
			 * their own embedded "next"s until those run
			 * out.  Each one's indirect flag must be off
			 * (we don't really have to check, could just
			 * ignore errors...).
			 */
			next = 0;
			for (;;) {
				vp = &vindir[next];
				if (vp->flags & VRING_DESC_F_INDIRECT) {
					printc("%s: indirect desc has INDIR flag,"
					    " driver confused?\r\n",
					    name);
					return -1;
				}
				if (_vq_record(i, vp, iov, n_iov, flags, vcpu)) {
					printc("%s: mapping to host failed\r\n", name);
					return -1;
				}
				if (++i > VQ_MAX_DESCRIPTORS)
					goto loopy;
				if ((vp->flags & VRING_DESC_F_NEXT) == 0)
					break;
				next = vp->next;
				if (next >= n_indir) {
					printc("%s: invalid next %u > %u, "
					    "driver confused?\r\n",
					    name, (unsigned int)next, n_indir);
					return -1;
				}
			}
		}
		if ((vdir->flags & VRING_DESC_F_NEXT) == 0)
			return i;
	}
loopy:
	printc("%s: descriptor loop? count > %d - driver confused?\r\n",
	    name, i);
	return -1;
}

void
vq_relchain(struct virtio_vq_info *vq, u16_t idx, u32_t iolen)
{
	u16_t uidx, mask;
	volatile struct vring_used *vuh;
	volatile struct vring_used_elem *vue;

	/*
	 * Notes:
	 *  - mask is N-1 where N is a power of 2 so computes x % N
	 *  - vuh points to the "used" data shared with guest
	 *  - vue points to the "used" ring entry we want to update
	 *  - head is the same value we compute in vq_iovecs().
	 *
	 * (I apologize for the two fields named idx; the
	 * virtio spec calls the one that vue points to, "id"...)
	 */
	mask = vq->qsize - 1;
	vuh = vq->used;

	uidx = vuh->idx;
	vue = &vuh->ring[uidx++ & mask];
	vue->id = idx;
	vue->len = iolen;
	vuh->idx = uidx;
}

static void
virtio_net_tap_rx(struct vmrt_vm_vcpu *vcpu)
{
	struct iovec iov[VIRTIO_NET_MAXSEGS], *riov;
	struct virtio_vq_info *vq;
	void *vrx;
	int len, n;
	u16_t idx;
	int ret;

	/*
	 * Check for available rx buffers
	 */
	vq = &virtio_net_vqs[VIRTIO_NET_RXQ];
	if (!vq_has_descs(vq)) {
		VM_PANIC(vcpu);
	}

	do {
		/*
		 * Get descriptor chain.
		 */
		n = vq_getchain(vcpu, vq, &idx, iov, VIRTIO_NET_MAXSEGS, NULL);
		if (n < 1 || n > VIRTIO_NET_MAXSEGS) {
			printc("vtnet: virtio_net_tap_rx: vq_getchain = %d\n", n);
			VM_PANIC(vcpu);
			return;
		}
		/*
		 * Get a pointer to the rx header, and use the
		 * data immediately following it for the packet buffer.
		 */
		vrx = iov[0].iov_base;
		riov = rx_iov_trim(iov, &n, sizeof(struct virtio_net_rxhdr));

		memcpy(iov[0].iov_base, icmp_reply, sizeof(icmp_reply));

		len = sizeof(icmp_reply);
		if (riov == NULL)
			VM_PANIC(vcpu);

		memset(vrx, 0, sizeof(struct virtio_net_rxhdr));
		

		if (len < 0 ) {
			/*
			 * No more packets, but still some avail ring
			 * entries.  Interrupt if needed/appropriate.
			 */
			/*TODO: fix the erro case */
			VM_PANIC(vcpu);
		}

		/*
		 * The only valid field in the rx packet header is the
		 * number of buffers if merged rx bufs were negotiated.
		 */

		/*
		 * Release this chain and handle more chains.
		 */
		vq_relchain(vq, idx, len);
	} while (vq_has_descs(vq));

	/* Interrupt if needed, including for NOTIFY_ON_EMPTY. */
	vq_endchains(vcpu, vq, 1);
}

static void
virtio_net_proctx(struct vmrt_vm_vcpu *vcpu, struct virtio_vq_info *vq)
{
	struct iovec iov[VIRTIO_NET_MAXSEGS + 1];
	int i, n;
	int plen, tlen;
	u16_t idx;

	memset(&iov, 0, sizeof(iov));
	/*
	 * Obtain chain of descriptors.  The first one is
	 * really the header descriptor, so we need to sum
	 * up two lengths: packet length and transfer length.
	 */
	n = vq_getchain(vcpu, vq, &idx, iov, VIRTIO_NET_MAXSEGS, NULL);
	if (n < 1 || n > VIRTIO_NET_MAXSEGS) {
		printc("vtnet: virtio_net_proctx: vq_getchain = %d\n", n);
		return;
	}
	plen = 0;
	tlen = iov[0].iov_len;
	for (i = 1; i < n; i++) {
		plen += iov[i].iov_len;
		tlen += iov[i].iov_len;
	}

	printc("virtio: packet send, %d bytes, %d segs\n\r", plen, n);
	/* begin to send packets */
	dump_packet(iov[1].iov_base, plen);
	/* TODO: process tx output to nic component */
	/* net->virtio_net_tx(net, &iov[1], n - 1, plen); */

	/* chain is processed, release it and set tlen */
	vq_relchain(vq, idx, tlen);
}

static void
virtio_net_tx_thread(void *param, struct vmrt_vm_vcpu *vcpu)
{
	struct virtio_vq_info *vq = &virtio_net_vqs[VIRTIO_NET_TXQ];

	while (!vq_ring_ready(vq))
		VM_PANIC(vcpu);

	vq->used->flags |= VRING_USED_F_NO_NOTIFY;

	do {
		/*
		* Run through entries, placing them into
		* iovecs and sending when an end-of-packet
		* is found
		*/
		virtio_net_proctx(vcpu, vq);
	} while (vq_has_descs(vq));
	printc("tx done on busrt\n");

	/*
	 * Generate an interrupt if needed.
	 */
	vq_endchains(vcpu, vq, 1);
}

static void
virtio_net_outb(u32_t port_id, struct vmrt_vm_vcpu *vcpu)
{
	u8_t val = vcpu->shared_region->ax;

	switch (port_id)
	{
	case VIRTIO_NET_DEV_STATUS:
		virtio_net_regs.header.dev_status = val;
		break;
	default:
		VM_PANIC(vcpu);
		break;
	}
	return;
}

static void 
virtio_net_inb(u32_t port_id, struct vmrt_vm_vcpu *vcpu)
{
	u32_t bdf = 0;

	switch (port_id)
	{
	case VIRTIO_NET_DEV_STATUS:
		vcpu->shared_region->ax = virtio_net_regs.header.dev_status;
		break;
	case VIRTIO_NET_ISR:
		vcpu->shared_region->ax = virtio_net_regs.header.ISR;
		break;
	case VIRTIO_NET_STATUS:
		vcpu->shared_region->ax = virtio_net_regs.config_reg.status; 
		break;
	case VIRTIO_NET_STATUS_H:
		vcpu->shared_region->ax = virtio_net_regs.config_reg.status >> 8; 
		break;
	/* TODO: read mac address from virtio-net config space */
	case VIRTIO_NET_MAC:
		vcpu->shared_region->ax = 0x10;
		break;
	case VIRTIO_NET_MAC1:
		vcpu->shared_region->ax = 0x10;
		break;
	case VIRTIO_NET_MAC2:
		vcpu->shared_region->ax = 0x10;
		break;
	case VIRTIO_NET_MAC3:
		vcpu->shared_region->ax = 0x10;
		break;
	case VIRTIO_NET_MAC4:
		vcpu->shared_region->ax = 0x10;
		break;
	case VIRTIO_NET_MAC5:
		vcpu->shared_region->ax = 0x11;
		break;
	default:
		VM_PANIC(vcpu);
		break;
	}
}

static void
virtio_net_inw(u32_t port_id, struct vmrt_vm_vcpu *vcpu)
{
	switch (port_id)
	{
	case VIRTIO_NET_QUEUE_SIZE:
		vcpu->shared_region->ax = virtio_queues[virtio_net_regs.header.queue_select].queue_sz;
		break;
	case VIRTIO_NET_QUEUE_SELECT:
		vcpu->shared_region->ax = virtio_net_regs.header.queue_select;
		break;
	case VIRTIO_NET_QUEUE_NOTIFY:
		VM_PANIC(vcpu);
		vcpu->shared_region->ax = virtio_net_regs.header.queue_notify;
		break;
	default:
		VM_PANIC(vcpu);
		break;
	}
}

static void
virtio_net_outw(u32_t port_id, struct vmrt_vm_vcpu *vcpu)
{
	u16_t val = vcpu->shared_region->ax;

	switch (port_id)
	{
	case VIRTIO_NET_QUEUE_SELECT:
		virtio_net_regs.header.queue_select = val;
		break;
	case VIRTIO_NET_QUEUE_NOTIFY:
		if (val == VIRTIO_NET_TXQ) {
			virtio_net_tx_thread(0, vcpu);
			virtio_net_tap_rx(vcpu);
		}
		virtio_net_regs.header.queue_notify = val;
		break;
	default:
		VM_PANIC(vcpu);
		break;
	}
}

static void
virtio_net_outl(u32_t port_id, struct vmrt_vm_vcpu *vcpu)
{
	u32_t val = vcpu->shared_region->ax;
	u64_t tmp = val;

	switch (port_id)
	{
	case VIRTIO_NET_GUEST_FEATURES:
		virtio_net_regs.header.guest_features = val;
		break;
	case VIRTIO_NET_QUEUE_ADDR:
		virtio_queues[virtio_net_regs.header.queue_select].queue = (void *)tmp;
		virtio_vq_init(vcpu, virtio_net_regs.header.queue_select, val);
		break;
	default:
		VM_PANIC(vcpu);
		break;
	}
	return;
}

static void
virtio_net_inl(u32_t port_id, struct vmrt_vm_vcpu *vcpu)
{
	switch (port_id)
	{
	case VIRTIO_NET_DEV_FEATURES:
		vcpu->shared_region->ax = virtio_net_regs.header.dev_features;
		break;
	case VIRTIO_NET_GUEST_FEATURES:
		vcpu->shared_region->ax = virtio_net_regs.header.guest_features;
		break;
	case VIRTIO_NET_QUEUE_ADDR:
		vcpu->shared_region->ax = virtio_net_regs.header.queue_addr;
		break;
	default:
		VM_PANIC(vcpu);
		break;
	}
}

void
virtio_net_handler(u16_t port, int dir, int sz, struct vmrt_vm_vcpu *vcpu)
{
	if (dir == IO_IN) {
		switch (sz)
		{
		case IO_BYTE:
			virtio_net_inb(port, vcpu);
			break;
		case IO_WORD:
			virtio_net_inw(port, vcpu);
			break;
		case IO_LONG:
			virtio_net_inl(port, vcpu);
			break;
		default:
			VM_PANIC(vcpu);
		}
	} else {
		switch (sz)
		{
		case IO_BYTE:
			virtio_net_outb(port, vcpu);
			break;
		case IO_WORD:
			virtio_net_outw(port, vcpu);
			break;
		case IO_LONG:
			virtio_net_outl(port, vcpu);
			break;
		default:
			VM_PANIC(vcpu);
		}
	}
}

void
virtio_net_io_init(void)
{
	memset(&virtio_net_regs, 0, sizeof(virtio_net_regs));
	memset(&virtio_queues, 0, sizeof(virtio_queues));
	memset(&virtio_net_vqs, 0, sizeof(virtio_net_vqs));

	virtio_net_regs.header.dev_features |= (1 << VIRTIO_NET_F_STATUS);
	virtio_net_regs.header.dev_features |= (1 << VIRTIO_NET_F_MAC);
	virtio_net_regs.config_reg.status = VIRTIO_NET_S_LINK_UP;
	virtio_queues[0].queue_sz = VQ_MAX_DESCRIPTORS;
	virtio_queues[1].queue_sz = VQ_MAX_DESCRIPTORS;
	virtio_net_vqs[VIRTIO_NET_RX].qsize = VQ_MAX_DESCRIPTORS;
	virtio_net_vqs[VIRTIO_NET_TX].qsize = VQ_MAX_DESCRIPTORS;
}