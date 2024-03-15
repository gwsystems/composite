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
#include <sync_lock.h>
#include <nf_session.h>
#include "virtio_net_io.h"
#include "vpci.h"
#include "virtio_ring.h"

#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <sync_sem.h>


/* TODO: remove this warning flag when virtio-net is done */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

static struct virtio_net_io_reg virtio_net_regs[VMRT_MAX_VM];
static struct virtio_queue virtio_queues[VMRT_MAX_VM][2];
static struct virtio_vq_info virtio_net_vqs[VMRT_MAX_VM][VIRTIO_NET_MAXQ - 1];

static inline struct virtio_net_io_reg *get_virtio_net_regs(struct vmrt_vm_vcpu *vcpu) {
	return &virtio_net_regs[vcpu->vm->comp_id];
}

static inline struct virtio_queue *get_virtio_queue(struct vmrt_vm_vcpu *vcpu, int nr_queue) {
	return &virtio_queues[vcpu->vm->comp_id][nr_queue];
}

static inline struct virtio_vq_info *get_virtio_net_vqs(struct vmrt_vm_comp *vm, int nr_queue) {
	return &virtio_net_vqs[vm->comp_id][nr_queue];
}

static u8_t virtio_net_mac[VMRT_MAX_VM][6] = {
		{0x10, 0x10, 0x10, 0x10, 0x10, 0x11}, 
		{0x10, 0x10, 0x10, 0x10, 0x10, 0x13},
		{0x10, 0x10, 0x10, 0x10, 0x10, 0x15},
		{0x10, 0x10, 0x10, 0x10, 0x10, 0x17},
		{0x10, 0x10, 0x10, 0x10, 0x10, 0x19},
		{0x10, 0x10, 0x10, 0x10, 0x10, 0x1a},
		{0x10, 0x10, 0x10, 0x10, 0x10, 0x1b},
		{0x10, 0x10, 0x10, 0x10, 0x10, 0x1c},
		{0x10, 0x10, 0x10, 0x10, 0x10, 0x1d},
		{0x10, 0x10, 0x10, 0x10, 0x10, 0x1e},
	};


static struct vmrt_vm_comp *mac_to_vm[VMRT_MAX_VM] = {0};

void
vm_mac_tbl_update(compid_t comp_id, struct vmrt_vm_comp *vm) {
	mac_to_vm[comp_id] = vm;
}

struct vmrt_vm_comp *get_vm_from_mac(u8_t *mac) {
	for (int i = 0; i < VMRT_MAX_VM; i++) {
		if(!memcmp(virtio_net_mac[i], mac, 6)) {
			return mac_to_vm[i];
		}
	}

	return NULL;
}


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
		get_virtio_net_regs(vcpu)->header.ISR = 1;
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

	vq = get_virtio_net_vqs(vcpu->vm, nr_queue);

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
	struct vmrt_vm_comp *vm = get_vm_from_mac(data);
	if (unlikely(vm == NULL)) {
		printc("vm not exist\n");
		return;
	}
	assert(vm);

	vq = get_virtio_net_vqs(vm, VIRTIO_NET_RXQ);
	
	vcpu = vq->vcpu;

	if (unlikely(!vq_has_descs(vq))) {
		return;
	}

	n = vq_getchain(vcpu, vq, &idx, iov, VIRTIO_NET_MAXSEGS, NULL);

	// if (unlikely (n < 1 || n >= VIRTIO_NET_MAXSEGS )) {
	// 	printc("vtnet: virtio_net_tap_rx: vq_getchain = %d\n", n);
	// 	assert(0);
	// }

	vrx = iov[0].iov_base;
	/* every packet needs to be proceeded by a virtio_net_rxhdr header space */
	riov = rx_iov_trim(iov, &n, sizeof(struct virtio_net_rxhdr));

	// assert(iov[0].iov_len >= (size_t)pkt_len);

	memcpy(iov[0].iov_base, data, pkt_len);

	memset(vrx, 0, sizeof(struct virtio_net_rxhdr));

	vq_relchain(vq, idx, pkt_len + sizeof(struct virtio_net_rxhdr));

	vq_endchains(vcpu, vq, 1);
	return;
}

void
virtio_net_send_one_pkt(void *data, u16_t *pkt_len, struct vmrt_vm_comp *vm)
{
	struct iovec iov[VIRTIO_NET_MAXSEGS];
	struct vmrt_vm_vcpu *vcpu;
	int i, n;
	int plen, tlen;
	u16_t idx;
	struct virtio_vq_info *vq;

	if (unlikely(vm == NULL)) {
		*pkt_len = 0;
		return;
	}

	vq = get_virtio_net_vqs(get_vm_from_mac(((u8_t *)data + 6)), VIRTIO_NET_TXQ);

	while (!vq_has_descs(vq)) {
		*pkt_len = 0;
		return;
		// sched_thd_block(0);
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

	memcpy(data, iov[1].iov_base, iov[1].iov_len);

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
		get_virtio_net_regs(vcpu)->header.dev_status = val;
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
		vcpu->shared_region->ax = get_virtio_net_regs(vcpu)->header.dev_status;
		break;
	case VIRTIO_NET_ISR:
		vcpu->shared_region->ax = get_virtio_net_regs(vcpu)->header.ISR;
		get_virtio_net_regs(vcpu)->header.ISR = 0;
		break;
	case VIRTIO_NET_STATUS:
		vcpu->shared_region->ax = get_virtio_net_regs(vcpu)->config_reg.status; 
		break;
	case VIRTIO_NET_STATUS_H:
		vcpu->shared_region->ax = get_virtio_net_regs(vcpu)->config_reg.status >> 8; 
		break;
	/* TODO: read mac address from virtio-net config space */
	case VIRTIO_NET_MAC:
		vm_mac_tbl_update(vcpu->vm->vm_mac_id, vcpu->vm);
		vcpu->shared_region->ax = virtio_net_mac[vcpu->vm->vm_mac_id][0];
		mac_to_vm[vcpu->vm->comp_id] = vcpu->vm;
		break;
	case VIRTIO_NET_MAC1:
		vcpu->shared_region->ax = virtio_net_mac[vcpu->vm->vm_mac_id][1];
		break;
	case VIRTIO_NET_MAC2:
		vcpu->shared_region->ax = virtio_net_mac[vcpu->vm->vm_mac_id][2];
		break;
	case VIRTIO_NET_MAC3:
		vcpu->shared_region->ax = virtio_net_mac[vcpu->vm->vm_mac_id][3];
		break;
	case VIRTIO_NET_MAC4:
		vcpu->shared_region->ax = virtio_net_mac[vcpu->vm->vm_mac_id][4];
		break;
	case VIRTIO_NET_MAC5:
		vcpu->shared_region->ax = virtio_net_mac[vcpu->vm->vm_mac_id][5];
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
		vcpu->shared_region->ax = get_virtio_queue(vcpu, get_virtio_net_regs(vcpu)->header.queue_select)->queue_sz;
		break;
	case VIRTIO_NET_QUEUE_SELECT:
		vcpu->shared_region->ax = get_virtio_net_regs(vcpu)->header.queue_select;
		break;
	case VIRTIO_NET_QUEUE_NOTIFY:
		VM_PANIC(vcpu);
		vcpu->shared_region->ax = get_virtio_net_regs(vcpu)->header.queue_notify;
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
		get_virtio_net_regs(vcpu)->header.queue_select = val;
		break;
	case VIRTIO_NET_QUEUE_NOTIFY:
		if (val == VIRTIO_NET_TXQ) {
			sched_thd_wakeup(vcpu->vm->tx_thd);
			// void virtio_tx_task(void *data);
			// virtio_tx_task(vcpu->vm);
		}
		get_virtio_net_regs(vcpu)->header.queue_notify = val;
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
		get_virtio_net_regs(vcpu)->header.guest_features = val;
		break;
	case VIRTIO_NET_QUEUE_ADDR:
		get_virtio_queue(vcpu, get_virtio_net_regs(vcpu)->header.queue_select)->queue = (void *)tmp;
		virtio_vq_init(vcpu, get_virtio_net_regs(vcpu)->header.queue_select, val);
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
		vcpu->shared_region->ax = get_virtio_net_regs(vcpu)->header.dev_features;
		break;
	case VIRTIO_NET_GUEST_FEATURES:
		vcpu->shared_region->ax = get_virtio_net_regs(vcpu)->header.guest_features;
		break;
	case VIRTIO_NET_QUEUE_ADDR:
		vcpu->shared_region->ax = get_virtio_net_regs(vcpu)->header.queue_addr;
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
virtio_tx_task(void *data)
{
	struct vmrt_vm_comp *vm = (struct vmrt_vm_comp *)data;
	// printc("virtio tx task :%p, id:%d, tx thd:%d\n", data, vm->comp_id, cos_thdid());

	struct virtio_vq_info *vq;
	struct iovec iov[VIRTIO_NET_MAXSEGS];
	struct vmrt_vm_vcpu *vcpu;
	int i, n;
	int plen, tlen;
	u16_t idx;
	char *pkt;
	struct netshmem_pkt_buf *tx_obj;
	shm_bm_objid_t tx_pktid;
	struct nf_pkt_meta_data buf;

	struct nf_session *session = NULL;
	shm_bm_t tx_shmemd = 0;
	u16_t svc_id = 0;

	struct iphdr *iphdr = NULL;
	struct tcphdr *tcphdr = NULL;
	struct udphdr *udphdr = NULL;

	vq = get_virtio_net_vqs(vm, VIRTIO_NET_TXQ);
	while (1) {
		while (!vq_has_descs(vq)) {
			sched_thd_block(0);
			// sched_thd_yield();
			// return;
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

		iphdr = (struct iphdr *) (pkt + ETH_HLEN);
		unsigned char *srcip = (unsigned char *)&(iphdr->saddr);
		unsigned char *dstip = (unsigned char *)&(iphdr->daddr);

		if(iphdr->protocol == IPPROTO_TCP)
		{
			tcphdr = (struct tcphdr*)((u_int8_t*)iphdr + (iphdr->ihl << 2));
			svc_id = ntohs(tcphdr->th_sport);
			svc_id = 0;
			// printc("tcp svc id:%u\n", svc_id);
			// length = pdataLen - (iphdr->ihl<<2) - (tcphdr->doff << 2);
			// payload = (u_int8_t*)( (u_int8_t*)tcphdr + (tcphdr->doff << 2) );
			// sport = tcphdr->source;
			// dport = tcphdr->dest;

			// printf("processed one packet:queue_num:%d, id:%d, plen:%d, ret:%d,src ip:%d.%d.%d.%d, dest ip:%d.%d.%d.%d, src port:%u, dst port:%u \n", queue_num, id, plen, ret, IP_ARG(srcip), IP_ARG(dstip), ntohs(tcphdr->source), ntohs(tcphdr->dest));
		} else if (iphdr->protocol == IPPROTO_UDP) {
			udphdr = (struct udphdr*)((u_int8_t*)iphdr + (iphdr->ihl << 2));
			svc_id = ntohs(udphdr->uh_sport);
			printc("udp svc id:%u\n", svc_id);
			assert(0);

	}
		session = get_nf_session(vm, svc_id);
		tx_shmemd = session->tx_shmemd;
		assert(tx_shmemd);

		if (tx_shmemd != 0) {
			tx_obj = shm_bm_alloc_net_pkt_buf(tx_shmemd, &tx_pktid);

			if (likely(tx_obj != NULL)){
				buf.objid = tx_pktid;
				buf.pkt_len = plen;
				buf.obj = tx_obj;
				memcpy(netshmem_get_data_buf(tx_obj), pkt, plen);
				if ((unlikely(!nf_tx_ring_buf_enqueue(&(session->nf_tx_ring_buf), &buf)))){
					shm_bm_free_net_pkt_buf(tx_obj);
					sched_thd_yield();
				} else {
					// sched_thd_wakeup(session->tx_thd);
					// sync_sem_give(&session->tx_sem);
				}
			} else {
				printc("cannot allocate objects\n");
				assert(0);
			}
		}

		vq_relchain(vq, idx, tlen);
		vq_endchains(vcpu, vq, 1);
	}
}

void
virtio_net_io_init(void)
{
	memset(&virtio_net_regs, 0, sizeof(virtio_net_regs));
	memset(&virtio_queues, 0, sizeof(virtio_queues));
	memset(&virtio_net_vqs, 0, sizeof(virtio_net_vqs));

	for(int i = 0; i < VMRT_MAX_VM; i++) {
		virtio_net_regs[i].header.dev_features |= (1 << VIRTIO_NET_F_STATUS);
		virtio_net_regs[i].header.dev_features |= (1 << VIRTIO_NET_F_MAC);
		virtio_net_regs[i].config_reg.status = VIRTIO_NET_S_LINK_UP;
		virtio_queues[i][0].queue_sz = VQ_MAX_DESCRIPTORS;
		virtio_queues[i][1].queue_sz = VQ_MAX_DESCRIPTORS;
		virtio_net_vqs[i][VIRTIO_NET_RX].qsize = VQ_MAX_DESCRIPTORS;
		virtio_net_vqs[i][VIRTIO_NET_TX].qsize = VQ_MAX_DESCRIPTORS;
	}
}