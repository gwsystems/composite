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
#include <sched.h>
#include <res_spec.h>
#include "virtio_net_io.h"
#include "vpci.h"
#include "virtio_ring.h"
#include <sync_lock.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <sync_sem.h>
#include <fast_memcpy.h>
#include <shm_bm.h>
#include <netshmem.h>
#include <nic_netio_tx.h>
#include <nic_netio_rx.h>
#include <nic_netio_shmem.h>
#include <arpa/inet.h>

/* Weak usage of nicmgr interface */
void __attribute__((weak)) nic_netio_shmem_map(cbuf_t shm_id);
int __attribute__((weak)) nic_netio_shmem_bind_port(u32_t ip_addr, u16_t port);
int __attribute__((weak)) nic_netio_tx_packet(shm_bm_objid_t pktid, u16_t pkt_len, u16_t len);
shm_bm_objid_t __attribute__((weak)) nic_netio_rx_packet(u16_t *pkt_len);


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
	return ((vq->flags & VQ_ALLOC) == VQ_ALLOC);
}

static inline int 
vq_has_descs(struct virtio_vq_info *vq)
{
	int ret = 0;
	if (vq_ring_ready(vq) && vq->last_avail != vq->avail->idx) {
		if (unlikely((u16_t)((unsigned int)vq->avail->idx - vq->last_avail) > vq->qsize)) {
			printc("no valid descriptor\n");
			assert(0);
		}
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

	if (unlikely(!vq || !vq->used))
		return;

	old_idx = vq->save_used;
	vq->save_used = new_idx = vq->used->idx;

	intr = new_idx != old_idx &&
		    !(vq->avail->flags & VRING_AVAIL_F_NO_INTERRUPT);
	
	if (intr) {
		virtio_net_regs.header.ISR = 1;
		/* TODO: 57 is virtio-net interrupt without io apic, 33 is with io apic */
#if VMX_SUPPORT_POSTED_INTR
		vlapic_set_intr(vcpu, 33, 0);
#else
		lapic_intr_inject(vcpu, 33, 0);
#endif
	}

}

static inline struct iovec *
rx_iov_trim(struct iovec *iov, int *niov, size_t tlen)
{
	struct iovec *riov;

	iov[0].iov_len -= tlen;
	
	iov[0].iov_base = (void *)((uintptr_t)iov[0].iov_base + tlen);
	riov = &iov[0];

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
	vq->vcpu = vcpu;

	return;

error:
	vq->flags = 0;
	printc("%s: vq enable failed\n", __func__);
	assert(0);
}

static inline int
_vq_record(int i, volatile struct vring_desc *vd,
	   struct iovec *iov, int n_iov, u16_t *flags, struct vmrt_vm_vcpu *vcpu) {

	void *host_addr;

	if (i >= n_iov) {
		printc("Number of descs is more than iov slots\n");
		assert(0);
	}

	host_addr = paddr_guest2host(vd->addr, vcpu->vm);
	if (unlikely(!host_addr)) {
		printc("Invalid host addr\n");
		assert(0);
	}
	iov[i].iov_base = host_addr;
	iov[i].iov_len = vd->len;
	if (unlikely(flags != NULL))
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
	if (unlikely(ndesc == 0))
		return 0;
	if (unlikely(ndesc > vq->qsize)) {
		printc("ndesc (%u) out of range, driver confused?\r\n",
		    (unsigned int)ndesc);
		assert(0);
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
			assert(0);
		}
		vdir = &vq->desc[next];
		if (likely((vdir->flags & VRING_DESC_F_INDIRECT) == 0)) {
			if (unlikely(_vq_record(i, vdir, iov, n_iov, flags, vcpu))) {
				printc("mapping to host failed\r\n");
				assert(0);
			}
			i++;
		} else {
			printc("descriptor has forbidden INDIRECT flag, "
			    "driver confused?\r\n");
			assert(0);
		}
		if ((vdir->flags & VRING_DESC_F_NEXT) == 0)
			return i;
	}

	/* code should not come here */
	printc("descriptor loop? count > %d - driver confused?\r\n", i);
	assert(0);
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

void
vq_retchain(struct virtio_vq_info *vq)
{
	vq->last_avail--;
}

void
virtio_net_rcv_one_pkt(void *data, int pkt_len)
{
	struct iovec iov[VIRTIO_NET_MAXSEGS], *riov;
	struct virtio_vq_info *vq;
	struct vmrt_vm_vcpu *vcpu;
	void *vrx;
	int n;
	u16_t idx;
	int ret;

	vq = &virtio_net_vqs[VIRTIO_NET_RXQ];
	
	vcpu = vq->vcpu;

	if (unlikely(!vq_has_descs(vq))) {
		return;
	}

	n = vq_getchain(vcpu, vq, &idx, iov, VIRTIO_NET_MAXSEGS, NULL);

	if (unlikely (n < 1 || n >= VIRTIO_NET_MAXSEGS )) {
		printc("vtnet: virtio_net_tap_rx: vq_getchain = %d\n", n);
	 	assert(0);
	}

	vrx = iov[0].iov_base;
	/* every packet needs to be proceeded by a virtio_net_rxhdr header space */
	riov = rx_iov_trim(iov, &n, sizeof(struct virtio_net_rxhdr));

	assert(iov[0].iov_len >= (size_t)pkt_len);

	// memcpy(iov[0].iov_base, data, pkt_len);
	memcpy_fast(iov[0].iov_base, data, pkt_len);

	memset(vrx, 0, sizeof(struct virtio_net_rxhdr));

	vq_relchain(vq, idx, pkt_len + sizeof(struct virtio_net_rxhdr));

	vq_endchains(vcpu, vq, 1);
	return;
}

void
virtio_net_send_one_pkt(void *data, u16_t *pkt_len)
{
	struct virtio_vq_info *vq = &virtio_net_vqs[VIRTIO_NET_TXQ];
	struct iovec iov[VIRTIO_NET_MAXSEGS];
	struct vmrt_vm_vcpu *vcpu;
	int i, n;
	int plen, tlen;
	u16_t idx;

	while (!vq_has_descs(vq)) {
		*pkt_len = 0;
		return;
		sched_thd_block(0);
	}
	vcpu = vq->vcpu;

	memset(&iov, 0, sizeof(iov));
	n = vq_getchain(vcpu, vq, &idx, iov, VIRTIO_NET_MAXSEGS, NULL);
	if (unlikely(n < 1 || n >= VIRTIO_NET_MAXSEGS)) {
		printc("vtnet: virtio_net_proctx: vq_getchain = %d\n", n);
		assert(0);
	}

	plen = 0;
	tlen = iov[0].iov_len;
	assert(n == 2);

	plen += iov[1].iov_len;
	tlen += iov[1].iov_len;

	// memcpy(data, iov[1].iov_base, iov[1].iov_len);
	memcpy_fast(data, iov[1].iov_base, iov[1].iov_len);

	*pkt_len = plen;
	vq_relchain(vq, idx, tlen);

	vq_endchains(vcpu, vq, 1);
	return;
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

#define REAL_NIC 0

static u8_t virtio_net_mac[6] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x11};

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
		virtio_net_regs.header.ISR = 0;
		break;
	case VIRTIO_NET_STATUS:
		vcpu->shared_region->ax = virtio_net_regs.config_reg.status; 
		break;
	case VIRTIO_NET_STATUS_H:
		vcpu->shared_region->ax = virtio_net_regs.config_reg.status >> 8; 
		break;
	/* TODO: read mac address from virtio-net config space */
	case VIRTIO_NET_MAC:
		vcpu->shared_region->ax = virtio_net_mac[0];
		break;
	case VIRTIO_NET_MAC1:
		vcpu->shared_region->ax = virtio_net_mac[1];
		break;
	case VIRTIO_NET_MAC2:
		vcpu->shared_region->ax = virtio_net_mac[2];
		break;
	case VIRTIO_NET_MAC3:
		vcpu->shared_region->ax = virtio_net_mac[3];
		break;
	case VIRTIO_NET_MAC4:
		vcpu->shared_region->ax = virtio_net_mac[4];
		break;
	case VIRTIO_NET_MAC5:
		vcpu->shared_region->ax = virtio_net_mac[5];
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
virtio_net_handler(u16_t io_port_addr, int dir, int sz, struct vmrt_vm_vcpu *vcpu)
{
	if (dir == IO_IN) {
		switch (sz)
		{
		case IO_BYTE:
			virtio_net_inb(io_port_addr, vcpu);
			break;
		case IO_WORD:
			virtio_net_inw(io_port_addr, vcpu);
			break;
		case IO_LONG:
			virtio_net_inl(io_port_addr, vcpu);
			break;
		default:
			VM_PANIC(vcpu);
		}
	} else {
		switch (sz)
		{
		case IO_BYTE:
			virtio_net_outb(io_port_addr, vcpu);
			break;
		case IO_WORD:
			virtio_net_outw(io_port_addr, vcpu);
			break;
		case IO_LONG:
			virtio_net_outl(io_port_addr, vcpu);
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

struct tcp_udp_port
{
	u16_t src_port;
	u16_t dst_port;
} __attribute__((packed));

void
virtio_tx_task(void *data)
{
	struct virtio_vq_info *vq;
	struct iovec iov[VIRTIO_NET_MAXSEGS];
	struct vmrt_vm_vcpu *vcpu;
	int i, n;
	int plen, tlen;
	u16_t idx;
	char *pkt;
	struct netshmem_pkt_buf *tx_obj;
	shm_bm_objid_t tx_pktid;
	shm_bm_t tx_shmemd = 0;
	int ret;
	cbuf_t shm_id;

	if (!nic_netio_shmem_map || !nic_netio_tx_packet)
	{
		printc("VMM TX: nicmgr interface not linked in, exiting TX task\n");
		assert(0);
	}

	printc("VMM TX: Task starting (tid=%lu)...\n", cos_thdid());
	
	netshmem_create(cos_thdid());

	shm_id = netshmem_get_shm_id();
	assert(shm_id != 0);

	tx_shmemd = netshmem_get_shm();
	assert(tx_shmemd);
	/* 
	 * Tell nicmgr about the shared memory.
	 * Note: nic_netio_shmem_map must be called BEFORE nic_netio_shmem_bind_port
	 * because bind_port expects the shmem to already be mapped in nicmgr's address space.
	 */
	nic_netio_shmem_map(shm_id);
	
	/* 
	 * Bind with IP=0 means this is a TX-only binding.
	 * service_port=1 is the network TCP/UDP port for packet routing.
	 * The nicmgr will use the mapped shared memory from the previous call.
	 */
	ret = nic_netio_shmem_bind_port(0, 1);
	if (ret < 0) {
		printc("VMM TX: Failed to bind port, ret=%d\n", ret);
		assert(0);
	}
	
	printc("VMM TX: Shared memory initialized, shmem_id=%u, bound to network service port 1\n", shm_id);

	vq = &virtio_net_vqs[VIRTIO_NET_TXQ];
	while (1) {
		while (!vq_has_descs(vq)) {
			sched_thd_yield();
		}

		vcpu = vq->vcpu;

		memset(&iov, 0, sizeof(iov));

		n = vq_getchain(vcpu, vq, &idx, iov, VIRTIO_NET_MAXSEGS, NULL);
		if (unlikely(n < 1 || n >= VIRTIO_NET_MAXSEGS)) {
			printc("vtnet: virtio_net_proctx: vq_getchain = %d\n", n);
			assert(0);
		}

		plen = 0;
		tlen = iov[0].iov_len;
		assert(n == 2);

		plen += iov[1].iov_len;
		tlen += iov[1].iov_len;

		pkt = iov[1].iov_base;

		/* Allocate packet buffer from shared memory */
		tx_obj = shm_bm_alloc_net_pkt_buf(tx_shmemd, &tx_pktid);

		if (likely(tx_obj != NULL)){
			/* Copy packet data to shared memory buffer */
			memcpy_fast(netshmem_get_data_buf(tx_obj), pkt, plen);
			
			/* Transmit to NIC */
			nic_netio_tx_packet(tx_pktid, netshmem_get_data_offset(), plen);
		} else {
			printc("cannot allocate tx objects\n");
			assert(0);
		}

		vq_relchain(vq, idx, tlen);
		vq_endchains(vcpu, vq, 1);
	}
}

void
virtio_rx_task(void *data)
{
	shm_bm_objid_t rx_pktid;
	u16_t pkt_len;
	struct netshmem_pkt_buf *rx_obj;
	shm_bm_t rx_shmemd = 0;
	int ret;
	cbuf_t shm_id;

	if (!nic_netio_shmem_map || !nic_netio_shmem_bind_port || !nic_netio_rx_packet)
	{
		printc("VMM RX: nicmgr interface not linked in, exiting RX task\n");
		assert(0);
	}
	printc("VMM RX: Task starting (tid=%lu)...\n", cos_thdid());
	
	netshmem_create(cos_thdid());

	shm_id = netshmem_get_shm_id();
	assert(shm_id != 0);
	
	rx_shmemd = netshmem_get_shm();
	assert(rx_shmemd);
	
	/* 
	 * Tell nicmgr about the shared memory.
	 * This maps the shmem into nicmgr's address space for this thread.
	 */
	nic_netio_shmem_map(shm_id);
	
	/* Bind this RX thread to IP address so nicmgr can route packets here.
	 * service_port=0 is the network TCP/UDP port for packet routing. */
	u32_t vm_ip = inet_addr("15.15.15.14");
	printc("VMM RX: Binding to IP 15.15.15.14 (0x%x), network service port 0\n", vm_ip);
	ret = nic_netio_shmem_bind_port(vm_ip, 0);
	if (ret < 0) {
		printc("VMM RX: Failed to bind to IP address, ret=%d\n", ret);
		assert(0);
	}
	printc("VMM RX: Successfully bound to IP 15.15.15.14, entering RX loop\n");
	assert(rx_shmemd);

	while (1) {
		/* Receive packet from NIC */
		rx_pktid = nic_netio_rx_packet(&pkt_len);
		
		if (rx_pktid == 0) {
			sched_thd_yield();
			continue;
		}

		/* Transfer ownership from NIC shared memory */
		rx_obj = shm_bm_transfer_net_pkt_buf(rx_shmemd, rx_pktid);
		if (unlikely(!rx_obj)) {
			printc("VMM RX: failed to transfer packet\n");
			continue;
		}

		/* Forward packet to guest VM */
		virtio_net_rcv_one_pkt(netshmem_get_data_buf(rx_obj), pkt_len);
		
		/* Free the packet buffer */
		shm_bm_free_net_pkt_buf(rx_obj);
	}
}
