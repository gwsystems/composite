/*
 ***********************************************************************  
 *  Gabriel Parmer <gabep1@bu.edu>
 *  Heavily modified tun/tap device driver.  Ripped out the character
 *  device, and custom fitted that end to composite.
 *
 *  ethtool -A eth0 autoneg off rx off tx off
 *  mknod /dev/net/cnet c 10 201
 *  echo 1 > /proc/sys/net/ipv4/ip_forward
 *  ./cnet_user (from the util dir)
 *  ifconfig cnet0 10.0.2.9 (done automatically)
 *
 *  and on the sending host:
 *  route add -net 10.0.2.0 gw masala netmask 255.255.255.0 eth0
 *  ./net/util/udp_client 10.0.2.8 200 10 1
 *
 ***********************************************************************  
 *  TUN - Universal TUN/TAP device driver.
 *  Copyright (C) 1999-2002 Maxim Krasnyansky <maxk@qualcomm.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  $Id: tun.c,v 1.15 2002/03/01 02:44:24 maxk Exp $
 */

/*
 *  Changes:
 *
 *  Brian Braunstein <linuxkernel@bristyle.com> 2007/03/23
 *    Fixed hw address handling.  Now net_device.dev_addr is kept consistent
 *    with tun.dev_addr when the address is set by this module.
 *
 *  Mike Kershaw <dragorn@kismetwireless.net> 2005/08/14
 *    Add TUNSETLINK ioctl to set the link encapsulation
 *
 *  Mark Smith <markzzzsmith@yahoo.com.au>
 *   Use random_ether_addr() for tap MAC address.
 *
 *  Harald Roelle <harald.roelle@ifi.lmu.de>  2004/04/20
 *    Fixes in packet dropping, queue length setting and queue wakeup.
 *    Increased default tx queue length.
 *    Added ethtool API.
 *    Minor cleanups
 *
 *  Daniel Podlejski <underley@underley.eu.org>
 *    Modifications for 2.3.99-pre5 kernel.
 */

#define DRV_NAME	"cnet"
#define DRV_VERSION	"1.6"
#define DRV_DESCRIPTION	"cos: Universal TUN/TAP device driver"
#define DRV_COPYRIGHT	"cos: (C) 1999-2004 Max Krasnyansky <maxk@qualcomm.com>"

#define COS_IP_ADDR     0xa000208

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/miscdevice.h>
#include <linux/ethtool.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/crc32.h>

#include "if_cosnet.h"//<linux/if_tun.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#ifdef TUN_DEBUG
static int debug;
#endif

/* Network device part of the driver */

static LIST_HEAD(tun_dev_list);
static const struct ethtool_ops tun_ethtool_ops;

/* Net device open. */
static int tun_net_open(struct net_device *dev)
{
	netif_start_queue(dev);
	return 0;
}

/* Net device close. */
static int tun_net_close(struct net_device *dev)
{
	netif_stop_queue(dev);
	return 0;
}

static inline struct cosnet_struct *cosnet_find_brand(struct tun_struct *ts, __u8 proto, __u16 dport)
{
	int i;
	struct cosnet_struct *cn;

	for (i = 0 ; i < COSNET_NUM_CHANNELS-1 ; i++) {
		struct cos_brand_info *bi = ts->cosnet[i].brand_info;

		if (dport && bi && bi->brand_port == dport) {
			return &ts->cosnet[i];
		}
	}
	/* The last entry is the wildcard */
	cn = &ts->cosnet[COSNET_NUM_CHANNELS-1];
	if (cn->brand_info && cn->brand_info->brand) {
		assert(cn->brand_info->brand_port == 0);
		return cn;
	}
	
	return NULL;
}

unsigned short int cosnet_skb_get_port(struct sk_buff *skb, __u8 *proto)
{
	__u32 daddr;
	__u16 dport;
	__u8  protocol, header_len;
	struct iphdr *iph = (struct iphdr*)skb->data;
	struct udphdr *udph;
	struct tcphdr *tcph;

	daddr = ntohl(iph->daddr);
	*proto = protocol = iph->protocol; /* udp is 17, tcp 6 */
	header_len = iph->ihl; /* size in words of ip header */

	if (daddr != COS_IP_ADDR || header_len != 5) {
		return 0;
	}

	switch (protocol) {
	case 0x11:
		udph = (struct udphdr *)(((long*)skb->data) + header_len);
		dport = ntohs(udph->dest);
		break;
	case 0x6:
		tcph = (struct tcphdr *)(((long*)skb->data) + header_len);
		dport = ntohs(tcph->dest);
		break;
	default:
		return 0;
	}

/* 	printk("cos: sip-%x, dip-%x, p-%x, len-%x, hl-%x, sport-%x,dport-%x,len-%x,cs-%x\n", */
/* 	       (unsigned int)ntohl(iph->saddr), (unsigned int)ntohl(iph->daddr),(unsigned int)iph->protocol, (unsigned int)ntohs(iph->tot_len), (unsigned int)header_len, */
/* 	       (unsigned int)ntohs(udph->source), (unsigned int)ntohs(udph->dest), (unsigned int)ntohs(udph->len), (unsigned int)ntohs(udph->check)); */
	return dport;
}

static struct cosnet_struct *cosnet_resolve_brand(struct tun_struct *ts, struct sk_buff *skb) {
	__u8 proto;
	__u16 port;

	port = cosnet_skb_get_port(skb, &proto);
	return cosnet_find_brand(ts, proto, port);
}

static void cosnet_init_queues(struct tun_struct *ts) 
{
	int i;

	for (i = 0 ; i < COSNET_NUM_CHANNELS ; i++) {
		struct cosnet_struct *cn = &ts->cosnet[i];

//		skb_queue_head_init(&cn->packet_queue);
		cn->brand_info = NULL;
		cn->packet_queue = NULL;
	}
}

static void cosnet_purge_queues(struct tun_struct *ts)
{
	int i;

	for (i = 0 ; i < COSNET_NUM_CHANNELS ; i++) {
		if (ts->cosnet[i].packet_queue) {
			skb_queue_purge(ts->cosnet[i].packet_queue);
		}
	}
}

/* be more aggressive here????? */
static int cosnet_queues_full(struct tun_struct *ts) 
{
	int i;

	for (i = 0 ; i < COSNET_NUM_CHANNELS ; i++) {
		if (ts->cosnet[i].packet_queue &&
		    skb_queue_len(ts->cosnet[i].packet_queue) < COSNET_QUEUE_LEN) {
			return 0;
		}
	}
	
	return 1;
}

struct tun_struct *local_ts = NULL;

/* 
 * Callback from composite.  Use this to setup a brand.
 */
int cosnet_create_brand(struct cos_brand_info *bi)
{
	int i;
	
	assert(local_ts && bi);

	/* Wildcard entry */
	if (bi->brand_port == 0) {
		struct cosnet_struct *cn = &local_ts->cosnet[COSNET_NUM_CHANNELS-1];

		printk("cos: cosnet - creating wild-card brand\n");
		cn->brand_info = bi;
		cn->packet_queue = kmalloc(sizeof(struct sk_buff_head), GFP_ATOMIC);
		skb_queue_head_init(cn->packet_queue);
		return 0;
	}
	for (i = 0 ; i < COSNET_NUM_CHANNELS-1 ; i++) {
		struct cos_brand_info *bi_tmp = local_ts->cosnet[i].brand_info;

		if (bi_tmp && bi_tmp->brand && bi_tmp->brand_port == bi->brand_port) {
			printk("cos: cosnet - re-wiring for port %d\n", bi->brand_port);
			local_ts->cosnet[i].brand_info = bi;
			local_ts->cosnet[i].packet_queue = kmalloc(sizeof(struct sk_buff_head), GFP_ATOMIC);
			skb_queue_head_init(local_ts->cosnet[i].packet_queue);
			return 0;
		}
		if (!bi_tmp) {
			printk("cos: cosnet - create brand for port %d\n", bi->brand_port);
			local_ts->cosnet[i].brand_info = bi;
			local_ts->cosnet[i].packet_queue = kmalloc(sizeof(struct sk_buff_head), GFP_ATOMIC);
			skb_queue_head_init(local_ts->cosnet[i].packet_queue);
			return 0;
		}
	}
	
	return -1;
}

int cosnet_remove_brand(struct cos_brand_info *bi)
{
	int i;

	assert(bi);

	for (i = 0 ; i < COSNET_NUM_CHANNELS ; i++) {
		if (local_ts->cosnet[i].brand_info == bi) {
			printk("cos: cosnet - remove brand for port %d\n", bi->brand_port);
			if (local_ts->cosnet[i].packet_queue) {
				skb_queue_purge(local_ts->cosnet[i].packet_queue);
				kfree(local_ts->cosnet[i].packet_queue);
			}
			local_ts->cosnet[i].brand_info = NULL;
			return 0;
		}
	}

	return -1;
}

/* 
 * Ok, these next two functions deserve an explanation: Because we
 * can't have the linker link function call points in the main cos
 * component to here (this module is dependent on it, not likewise),
 * we need to use callback.  So the main composite module calls this
 * to get a packet, so we dequeue a packet and return the skb's data.
 * But when do we deallocate the skb?  If we do it immediately, then
 * the data we pass to the main module could get corrupted (using
 * freed memory).  This we introduce the callback that the main module
 * must call when done with the data to deallocate the skbuff.
 */

/* of type cos_net_data_completion_t */
void cosnet_skb_completion(void *data)
{
	struct sk_buff *skb = data;

	BUG_ON(!skb);

	kfree_skb(skb);
}

/* 
 * Callback from composite.  This is a request to get an item from a
 * brand-specific packet queue.  
 */
int cosnet_get_packet(struct cos_brand_info *bi, char **packet, unsigned long *len, 
		      cos_net_data_completion_t *fn, void **data, unsigned short int *port)
{
	int i;

	assert(local_ts);

	for (i = 0 ; i < COSNET_NUM_CHANNELS ; i++) {
		struct cos_brand_info *tmp_bi = local_ts->cosnet[i].brand_info;
		if (tmp_bi && tmp_bi->brand_port == bi->brand_port) {
			struct sk_buff *skb;
			__u8 proto;
			//int queues_full = cosnet_queues_full(local_ts);

			if (!(skb = skb_dequeue(local_ts->cosnet[i].packet_queue))) {
				/* This should NOT happen */
				printk("cos: composite asking for packet, and none there!  "
				       "Inconsistency between packet queue, and pending brands.\n");
				BUG();
			}

			/* TODO: restart the queue with netif_wake_queue */

			*port = cosnet_skb_get_port(skb, &proto);
			/* OK, this is a little rediculous */
			*len = skb->len;
			*packet = skb->data;
			*fn = cosnet_skb_completion;
			*data = (void*)skb;
//			if (queues_full) {
				//netif_wake_queue(local_ts->dev);
//			}
			local_ts->stats.rx_packets++;

			return 0;
		}
	}
	
	return 1;
} 

extern void host_start_syscall(void);
extern void host_end_syscall(void);

static int cosnet_xmit_packet(void *headers, int hlen, struct gather_item *gi, 
			      int gi_len, int tot_gi_len)
{
	struct sk_buff *skb;
	int totlen = hlen + tot_gi_len, i;
#ifdef NIL
	int prev_pending = 0;
#endif
	if (!(skb = alloc_skb(totlen /* + NET_IP_ALIGN */, GFP_ATOMIC))) {
		local_ts->stats.tx_dropped++;
		return -1;
	}
	if (unlikely(totlen > local_ts->dev->mtu || 
		     hlen > sizeof(struct cos_net_xmit_headers))) {
		printk("cos: cannot transfer packet of size %d.\n", totlen);
		kfree_skb(skb);
		return -1;
	}
	/* skb_reserve(skb, NET_IP_ALIGN); */
	/* Copy the headers first */
	memcpy(skb_put(skb, hlen), headers, hlen);
	/* Then the data payload */
	for (i = 0 ; i < gi_len ; i++) {
		struct gather_item *curr_gi = &gi[i];
		memcpy(skb_put(skb, curr_gi->len), curr_gi->data, curr_gi->len);
	}
	
/* 	{ */
/* 		struct iphdr *ih = (struct iphdr *)skb->data; */
/* 		int len = ih->ihl; */
/* 		struct udphdr *uh = (struct udphdr *)((long *)skb->data + len); */

/* 		printk("ih len %d, udp hdr: sp %d, dp %d, len %d\n", len, ntohs(uh->source), ntohs(uh->dest), ntohs(uh->len)); */
/* 	} */

	skb_reset_mac_header(skb);
	skb->protocol = __constant_htons(ETH_P_IP);
	skb->dev = local_ts->dev;
	skb->ip_summed = CHECKSUM_UNNECESSARY;

#ifdef NIL
	if (local_softirq_pending()) {
		prev_pending = 1;
		printk("cos pending softirq before xmit!\n");
	}
#endif
	if (unlikely(!raw_irqs_disabled())) {
		printk("cos: interrupts enabled before packet xmit!\n");
		kfree_skb(skb);
		return -1;
	}

	if (NET_RX_DROP == netif_rx/*_ni*/(skb)) {
		local_ts->stats.tx_dropped++;
	} else {
		local_ts->dev->last_rx = jiffies;
		local_ts->stats.tx_packets++;
		local_ts->stats.tx_bytes += totlen;
	}
	if (unlikely(!raw_irqs_disabled())) {
		printk("cos: interrupts enabled after packet xmit!\n");
		kfree_skb(skb);
		return -1;
	}

	/**  
	 * A lot of explanation is needed here.  Sending data onto the
	 * network will cause the triggering of a softirq.  If
	 * softirqs are executed immediately, then a hard halt results
	 * as the packet receive softirq triggers which attempts to
	 * get the user's registers from the stack and change them...
	 * Because we are executing in a composite system call,
	 * registers are not saved on the stack on kernel entry as
	 * they are in Linux.  Thus, crash.  So we set a global
	 * variable notifying the network receive handler (and timer
	 * interrupt), that they cannot really switch.  This will
	 * delay the reception of that event, and that is something we
	 * will live with for now.  To that, I say FIXME.
	 *
	 * 12/08 - Another possible reason this is happening: enough
	 * softirqs are happening that we get X (e.g. 10) in a row
	 * which triggers Linux's delayed softirq behavior (softirqd)
	 * where softirq execution is deferred into a thread context.
	 * As we are running at the highest priority, that thread will
	 * never run.  That is how local_softirq_pending() == 1 even
	 * though we should be operating with irqs disabled in the
	 * entire xmit path.  This would explain not why there are
	 * softirqs pending (as there should be xmit ones after
	 * transmission), instead it explains why there are receive
	 * softirqs pending.
	 *
	 * I could have instead called netif_rx_ni and had the
	 * host_*_syscall around it, but this is more transparent.
	 */
	if (local_softirq_pending()) {
		host_start_syscall();
		do_softirq();
		host_end_syscall();
	}
	if (unlikely(!raw_irqs_disabled())) {
		printk("cos: interrupts enabled after softirq in xmit!\n");
		kfree_skb(skb);
		return -1;
	}

	return 0;
}

extern void cos_net_prebrand(void);
extern int  cos_net_try_brand(struct thread *bi, void *data, int len);
extern void cos_net_register(struct cos_net_callbacks *cn_cb);
extern void cos_net_deregister(struct cos_net_callbacks *cn_cb);
extern int cos_net_notify_drop(struct thread *brand);

struct cos_net_callbacks cosnet_cbs = {
	.get_packet = cosnet_get_packet,
	.xmit_packet  = cosnet_xmit_packet,
	.create_brand = cosnet_create_brand,
	.remove_brand = cosnet_remove_brand
};

static int cosnet_cos_register(void)
{
	cos_net_register(&cosnet_cbs);
	
	return 0;
}

static int cosnet_cos_deregister(void)
{
	cos_net_deregister(&cosnet_cbs);

	return 0;
}

/* 
 * If the brand is made, and the skb is superfluous, return 1,
 * otherwise, return 0 (thus the skb needs to be queued).
 */
static int cosnet_execute_brand(struct cos_brand_info *brand, struct sk_buff *skb)
{
	assert(brand && skb);
	return cos_net_try_brand(brand->brand, (void*)skb->data, skb->len);
}

/* Net device start xmit */
static int tun_net_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct tun_struct *tun = netdev_priv(dev);
	struct cosnet_struct *cosnet;

	DBG(KERN_INFO "%s: tun_net_xmit %d\n", tun->dev->name, skb->len);

	/* Drop packet if interface is not attached */
	//if (!tun->attached)
	//goto drop;

	cos_net_prebrand();
	cosnet = cosnet_resolve_brand(tun, skb);
	if (!cosnet) {
		/* Don't count packets that arrive before or after cos runs */
		kfree_skb(skb);
		return 0;
		//printk("cos: could not resolve brand for packet.\n");
		//goto drop;
	}

	/* Packet dropping */
/*	if (cosnet_queues_full(tun)) {
		netif_stop_queue(dev);
		tun->stats.tx_fifo_errors++;
		goto drop;
	}
*/
	if (!cosnet->packet_queue) {
		printk("cos: packet queue not set up for brand.\n");
		goto drop;
	}
	if (skb_queue_len(cosnet->packet_queue) >= COSNET_QUEUE_LEN) {
		//printk("cos: NET->overflowing packet queue.\n");
		//cos_net_notify_drop(cosnet->brand_info->brand);
		printk("cos: WTF, kernel packet queue full...inexplicable\n");
		goto drop;
	}

	/* Queue packet */
	/* Ring buffer is going to take the part of the skb queue
	   skb_queue_tail(cosnet->packet_queue, skb);
	*/

	if (cosnet_execute_brand(cosnet->brand_info, skb)) {
		goto drop;
	}
	/* end crit section here? */
	dev->trans_start = jiffies;
	tun->stats.rx_packets++;
	tun->stats.rx_bytes += skb->len;
	/*
	 * Ring buffers should be used now instead of skb queues, so
	 * we can delete here.
	 */
	kfree_skb(skb);
	/* Notify and wake up reader process */
/* 	if (tun->flags & TUN_FASYNC) */
/* 		kill_fasync(&tun->fasync, SIGIO, POLL_IN); */
/* 	wake_up_interruptible(&tun->read_wait); */
	return 0;

drop:
	tun->stats.rx_dropped++;
	kfree_skb(skb);
	return 0;
}

/** Add the specified Ethernet address to this multicast filter. */
static void
add_multi(u32* filter, const u8* addr)
{
	int bit_nr = ether_crc(ETH_ALEN, addr) >> 26;
	filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
}

/** Remove the specified Ethernet addres from this multicast filter. */
static void
del_multi(u32* filter, const u8* addr)
{
	int bit_nr = ether_crc(ETH_ALEN, addr) >> 26;
	filter[bit_nr >> 5] &= ~(1 << (bit_nr & 31));
}

/** Update the list of multicast groups to which the network device belongs.
 * This list is used to filter packets being sent from the character device to
 * the network device. */
static void
tun_net_mclist(struct net_device *dev)
{
	struct tun_struct *tun = netdev_priv(dev);
	const struct dev_mc_list *mclist;
	int i;
	DBG(KERN_DEBUG "%s: tun_net_mclist: mc_count %d\n",
			dev->name, dev->mc_count);
	memset(tun->chr_filter, 0, sizeof tun->chr_filter);
	for (i = 0, mclist = dev->mc_list; i < dev->mc_count && mclist != NULL;
			i++, mclist = mclist->next) {
		add_multi(tun->net_filter, mclist->dmi_addr);
		DBG(KERN_DEBUG "%s: tun_net_mclist: %x:%x:%x:%x:%x:%x\n",
				dev->name,
				mclist->dmi_addr[0], mclist->dmi_addr[1], mclist->dmi_addr[2],
				mclist->dmi_addr[3], mclist->dmi_addr[4], mclist->dmi_addr[5]);
	}
}

static struct net_device_stats *tun_net_stats(struct net_device *dev)
{
	struct tun_struct *tun = netdev_priv(dev);
	return &tun->stats;
}

/* Initialize net device. */
static void tun_net_init(struct net_device *dev)
{
	struct tun_struct *tun = netdev_priv(dev);

	switch (tun->flags & TUN_TYPE_MASK) {
	case TUN_TUN_DEV:
		/* Point-to-Point TUN Device */
		dev->hard_header_len = 0;
		dev->addr_len = 0;
		dev->mtu = 1500;

		/* Zero header length */
		dev->type = ARPHRD_NONE;
		dev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;
		dev->tx_queue_len = TUN_READQ_SIZE;  /* We prefer our own queue length */
		break;

	case TUN_TAP_DEV:
		/* Ethernet TAP Device */
		dev->set_multicast_list = tun_net_mclist;

		ether_setup(dev);

		/* random address already created for us by tun_set_iff, use it */
		memcpy(dev->dev_addr, tun->dev_addr, min(sizeof(tun->dev_addr), sizeof(dev->dev_addr)) );

		dev->tx_queue_len = TUN_READQ_SIZE;  /* We prefer our own queue length */
		break;
	}
}

/* Character device part */

/* Poll */
static unsigned int tun_chr_poll(struct file *file, poll_table * wait)
{
	struct tun_struct *tun = file->private_data;
	unsigned int mask = POLLOUT | POLLWRNORM;

	if (!tun)
		return -EBADFD;

	DBG(KERN_INFO "%s: tun_chr_poll\n", tun->dev->name);

	poll_wait(file, &tun->read_wait, wait);

	if (!skb_queue_empty(&tun->readq))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

/* Get packet from user space buffer */
static __inline__ ssize_t tun_get_user(struct tun_struct *tun, struct iovec *iv, size_t count)
{
	struct tun_pi pi = { 0, __constant_htons(ETH_P_IP) };
	struct sk_buff *skb;
	size_t len = count, align = 0;

	if (!(tun->flags & TUN_NO_PI)) {
		if ((len -= sizeof(pi)) > count)
			return -EINVAL;

		if(memcpy_fromiovec((void *)&pi, iv, sizeof(pi)))
			return -EFAULT;
	}

	if ((tun->flags & TUN_TYPE_MASK) == TUN_TAP_DEV)
		align = NET_IP_ALIGN;

	if (!(skb = alloc_skb(len + align, GFP_KERNEL))) {
		tun->stats.rx_dropped++;
		return -ENOMEM;
	}

	if (align)
		skb_reserve(skb, align);
	if (memcpy_fromiovec(skb_put(skb, len), iv, len)) {
		tun->stats.rx_dropped++;
		kfree_skb(skb);
		return -EFAULT;
	}

	switch (tun->flags & TUN_TYPE_MASK) {
	case TUN_TUN_DEV:
		skb_reset_mac_header(skb);
		skb->protocol = pi.proto;
		skb->dev = tun->dev;
		break;
	case TUN_TAP_DEV:
		skb->protocol = eth_type_trans(skb, tun->dev);
		break;
	};

	if (tun->flags & TUN_NOCHECKSUM)
		skb->ip_summed = CHECKSUM_UNNECESSARY;

	netif_rx_ni(skb);
	tun->dev->last_rx = jiffies;

	tun->stats.rx_packets++;
	tun->stats.rx_bytes += len;

	return count;
}

static inline size_t iov_total(const struct iovec *iv, unsigned long count)
{
	unsigned long i;
	size_t len;

	for (i = 0, len = 0; i < count; i++)
		len += iv[i].iov_len;

	return len;
}

static ssize_t tun_chr_aio_write(struct kiocb *iocb, const struct iovec *iv,
			      unsigned long count, loff_t pos)
{
	struct tun_struct *tun = iocb->ki_filp->private_data;

	if (!tun)
		return -EBADFD;

	DBG(KERN_INFO "%s: tun_chr_write %ld\n", tun->dev->name, count);

	return tun_get_user(tun, (struct iovec *) iv, iov_total(iv, count));
}

/* Put packet to the user space buffer */
static __inline__ ssize_t tun_put_user(struct tun_struct *tun,
				       struct sk_buff *skb,
				       struct iovec *iv, int len)
{
	struct tun_pi pi = { 0, skb->protocol };
	ssize_t total = 0;

	if (!(tun->flags & TUN_NO_PI)) {
		if ((len -= sizeof(pi)) < 0)
			return -EINVAL;

		if (len < skb->len) {
			/* Packet will be striped */
			pi.flags |= TUN_PKT_STRIP;
		}

		if (memcpy_toiovec(iv, (void *) &pi, sizeof(pi)))
			return -EFAULT;
		total += sizeof(pi);
	}

	len = min_t(int, skb->len, len);

	skb_copy_datagram_iovec(skb, 0, iv, len);
	total += len;

	tun->stats.tx_packets++;
	tun->stats.tx_bytes += len;

	return total;
}

static ssize_t tun_chr_aio_read(struct kiocb *iocb, const struct iovec *iv,
			    unsigned long count, loff_t pos)
{
	struct file *file = iocb->ki_filp;
	struct tun_struct *tun = file->private_data;
	DECLARE_WAITQUEUE(wait, current);
	struct sk_buff *skb;
	ssize_t len, ret = 0;

	if (!tun)
		return -EBADFD;

	DBG(KERN_INFO "%s: tun_chr_read\n", tun->dev->name);

	len = iov_total(iv, count);
	if (len < 0)
		return -EINVAL;

	add_wait_queue(&tun->read_wait, &wait);
	while (len) {
		const u8 ones[ ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
		u8 addr[ ETH_ALEN];
		int bit_nr;

		current->state = TASK_INTERRUPTIBLE;

		/* Read frames from the queue */
		if (!(skb=skb_dequeue(&tun->readq))) {
			if (file->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				break;
			}
			if (signal_pending(current)) {
				ret = -ERESTARTSYS;
				break;
			}

			/* Nothing to read, let's sleep */
			schedule();
			continue;
		}
		netif_wake_queue(tun->dev);

		/** Decide whether to accept this packet. This code is designed to
		 * behave identically to an Ethernet interface. Accept the packet if
		 * - we are promiscuous.
		 * - the packet is addressed to us.
		 * - the packet is broadcast.
		 * - the packet is multicast and
		 *   - we are multicast promiscous.
		 *   - we belong to the multicast group.
		 */
		skb_copy_from_linear_data(skb, addr, min_t(size_t, sizeof addr,
								   skb->len));
		bit_nr = ether_crc(sizeof addr, addr) >> 26;
		if ((tun->if_flags & IFF_PROMISC) ||
				memcmp(addr, tun->dev_addr, sizeof addr) == 0 ||
				memcmp(addr, ones, sizeof addr) == 0 ||
				(((addr[0] == 1 && addr[1] == 0 && addr[2] == 0x5e) ||
				  (addr[0] == 0x33 && addr[1] == 0x33)) &&
				 ((tun->if_flags & IFF_ALLMULTI) ||
				  (tun->chr_filter[bit_nr >> 5] & (1 << (bit_nr & 31)))))) {
			DBG(KERN_DEBUG "%s: tun_chr_readv: accepted: %x:%x:%x:%x:%x:%x\n",
					tun->dev->name, addr[0], addr[1], addr[2],
					addr[3], addr[4], addr[5]);
			ret = tun_put_user(tun, skb, (struct iovec *) iv, len);
			kfree_skb(skb);
			break;
		} else {
			DBG(KERN_DEBUG "%s: tun_chr_readv: rejected: %x:%x:%x:%x:%x:%x\n",
					tun->dev->name, addr[0], addr[1], addr[2],
					addr[3], addr[4], addr[5]);
			kfree_skb(skb);
			continue;
		}
	}

	current->state = TASK_RUNNING;
	remove_wait_queue(&tun->read_wait, &wait);

	return ret;
}

static void tun_setup(struct net_device *dev)
{
	struct tun_struct *tun = netdev_priv(dev);

	skb_queue_head_init(&tun->readq);
	cosnet_init_queues(tun);
	init_waitqueue_head(&tun->read_wait);

	tun->owner = -1;

	SET_MODULE_OWNER(dev);
	dev->open = tun_net_open;
	dev->hard_start_xmit = tun_net_xmit;
	dev->stop = tun_net_close;
	dev->get_stats = tun_net_stats;
	dev->ethtool_ops = &tun_ethtool_ops;
	dev->destructor = free_netdev;
}

static struct tun_struct *tun_get_by_name(const char *name)
{
	struct tun_struct *tun;

	ASSERT_RTNL();
	list_for_each_entry(tun, &tun_dev_list, list) {
		if (!strncmp(tun->dev->name, name, IFNAMSIZ))
		    return tun;
	}

	return NULL;
}

static int tun_set_iff(struct file *file, struct ifreq *ifr)
{
	struct tun_struct *tun;
	struct net_device *dev;
	int err;

	tun = tun_get_by_name(ifr->ifr_name);
	if (tun) {
		//if (tun->attached)
		//	return -EBUSY;

		/* Check permissions */
		if (tun->owner != -1 &&
		    current->euid != tun->owner && !capable(CAP_NET_ADMIN))
			return -EPERM;
	}
	else if (__dev_get_by_name(ifr->ifr_name))
		return -EINVAL;
	else {
		char *name;
		unsigned long flags = 0;

		err = -EINVAL;

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		/* Set dev type */
		if (ifr->ifr_flags & IFF_TUN) {
			/* TUN device */
			flags |= TUN_TUN_DEV;
			name = "cnet%d";
		} else if (ifr->ifr_flags & IFF_TAP) {
			/* TAP device */
			flags |= TUN_TAP_DEV;
			name = "cnet%d";
		} else
			goto failed;

		if (*ifr->ifr_name)
			name = ifr->ifr_name;

		dev = alloc_netdev(sizeof(struct tun_struct), name,
				   tun_setup);
		if (!dev)
			return -ENOMEM;

		tun = netdev_priv(dev);
		tun->dev = dev;
		tun->flags = flags;
		/* Be promiscuous by default to maintain previous behaviour. */
		tun->if_flags = IFF_PROMISC;
		/* Generate random Ethernet address. */
		*(u16 *)tun->dev_addr = htons(0x00FF);
		get_random_bytes(tun->dev_addr + sizeof(u16), 4);
		memset(tun->chr_filter, 0, sizeof tun->chr_filter);

		tun_net_init(dev);

		if (strchr(dev->name, '%')) {
			err = dev_alloc_name(dev, dev->name);
			if (err < 0)
				goto err_free_dev;
		}

		err = register_netdevice(tun->dev);
		if (err < 0)
			goto err_free_dev;

		list_add(&tun->list, &tun_dev_list);
	}

	DBG(KERN_INFO "%s: tun_set_iff\n", tun->dev->name);

	if (ifr->ifr_flags & IFF_NO_PI)
		tun->flags |= TUN_NO_PI;

	if (ifr->ifr_flags & IFF_ONE_QUEUE)
		tun->flags |= TUN_ONE_QUEUE;

	file->private_data = tun;
	/* gabep1 */
	local_ts = tun;
	//tun->attached = 1;

	strcpy(ifr->ifr_name, tun->dev->name);
	return 0;

 err_free_dev:
	free_netdev(dev);
 failed:
	return err;
}

static int tun_chr_ioctl(struct inode *inode, struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	struct tun_struct *tun = file->private_data;
	void __user* argp = (void __user*)arg;
	struct ifreq ifr;

	if (cmd == TUNSETIFF || _IOC_TYPE(cmd) == 0x89)
		if (copy_from_user(&ifr, argp, sizeof ifr))
			return -EFAULT;

	if (cmd == TUNSETIFF && !tun) {
		int err;

		ifr.ifr_name[IFNAMSIZ-1] = '\0';

		rtnl_lock();
		err = tun_set_iff(file, &ifr);
		rtnl_unlock();

		if (err)
			return err;

		if (copy_to_user(argp, &ifr, sizeof(ifr)))
			return -EFAULT;
		return 0;
	}

	if (!tun)
		return -EBADFD;

	DBG(KERN_INFO "%s: tun_chr_ioctl cmd %d\n", tun->dev->name, cmd);

	switch (cmd) {
	case TUNSETNOCSUM:
		/* Disable/Enable checksum */
		if (arg)
			tun->flags |= TUN_NOCHECKSUM;
		else
			tun->flags &= ~TUN_NOCHECKSUM;

		DBG(KERN_INFO "%s: checksum %s\n",
		    tun->dev->name, arg ? "disabled" : "enabled");
		break;

	case TUNSETPERSIST:
		/* Disable/Enable persist mode */
		if (arg)
			tun->flags |= TUN_PERSIST;
		else
			tun->flags &= ~TUN_PERSIST;

		DBG(KERN_INFO "%s: persist %s\n",
		    tun->dev->name, arg ? "disabled" : "enabled");
		break;

	case TUNSETOWNER:
		/* Set owner of the device */
		tun->owner = (uid_t) arg;

		DBG(KERN_INFO "%s: owner set to %d\n", tun->dev->name, tun->owner);
		break;

	case TUNSETLINK:
		/* Only allow setting the type when the interface is down */
		if (tun->dev->flags & IFF_UP) {
			DBG(KERN_INFO "%s: Linktype set failed because interface is up\n",
				tun->dev->name);
			return -EBUSY;
		} else {
			tun->dev->type = (int) arg;
			DBG(KERN_INFO "%s: linktype set to %d\n", tun->dev->name, tun->dev->type);
		}
		break;

#ifdef TUN_DEBUG
	case TUNSETDEBUG:
		tun->debug = arg;
		break;
#endif

	case SIOCGIFFLAGS:
		ifr.ifr_flags = tun->if_flags;
		if (copy_to_user( argp, &ifr, sizeof ifr))
			return -EFAULT;
		return 0;

	case SIOCSIFFLAGS:
		/** Set the character device's interface flags. Currently only
		 * IFF_PROMISC and IFF_ALLMULTI are used. */
		tun->if_flags = ifr.ifr_flags;
		DBG(KERN_INFO "%s: interface flags 0x%lx\n",
				tun->dev->name, tun->if_flags);
		return 0;

	case SIOCGIFHWADDR:
		/* Note: the actual net device's address may be different */
		memcpy(ifr.ifr_hwaddr.sa_data, tun->dev_addr,
				min(sizeof ifr.ifr_hwaddr.sa_data, sizeof tun->dev_addr));
		if (copy_to_user( argp, &ifr, sizeof ifr))
			return -EFAULT;
		return 0;

	case SIOCSIFHWADDR:
	{
		/* try to set the actual net device's hw address */
		int ret = dev_set_mac_address(tun->dev, &ifr.ifr_hwaddr);

		if (ret == 0) {
			/** Set the character device's hardware address. This is used when
			 * filtering packets being sent from the network device to the character
			 * device. */
			memcpy(tun->dev_addr, ifr.ifr_hwaddr.sa_data,
					min(sizeof ifr.ifr_hwaddr.sa_data, sizeof tun->dev_addr));
			DBG(KERN_DEBUG "%s: set hardware address: %x:%x:%x:%x:%x:%x\n",
					tun->dev->name,
					tun->dev_addr[0], tun->dev_addr[1], tun->dev_addr[2],
					tun->dev_addr[3], tun->dev_addr[4], tun->dev_addr[5]);
		}

		return  ret;
	}

	case SIOCADDMULTI:
		/** Add the specified group to the character device's multicast filter
		 * list. */
		add_multi(tun->chr_filter, ifr.ifr_hwaddr.sa_data);
		DBG(KERN_DEBUG "%s: add multi: %x:%x:%x:%x:%x:%x\n",
				tun->dev->name,
				(u8)ifr.ifr_hwaddr.sa_data[0], (u8)ifr.ifr_hwaddr.sa_data[1],
				(u8)ifr.ifr_hwaddr.sa_data[2], (u8)ifr.ifr_hwaddr.sa_data[3],
				(u8)ifr.ifr_hwaddr.sa_data[4], (u8)ifr.ifr_hwaddr.sa_data[5]);
		return 0;

	case SIOCDELMULTI:
		/** Remove the specified group from the character device's multicast
		 * filter list. */
		del_multi(tun->chr_filter, ifr.ifr_hwaddr.sa_data);
		DBG(KERN_DEBUG "%s: del multi: %x:%x:%x:%x:%x:%x\n",
				tun->dev->name,
				(u8)ifr.ifr_hwaddr.sa_data[0], (u8)ifr.ifr_hwaddr.sa_data[1],
				(u8)ifr.ifr_hwaddr.sa_data[2], (u8)ifr.ifr_hwaddr.sa_data[3],
				(u8)ifr.ifr_hwaddr.sa_data[4], (u8)ifr.ifr_hwaddr.sa_data[5]);
		return 0;

	default:
		return -EINVAL;
	};

	return 0;
}

static int tun_chr_fasync(int fd, struct file *file, int on)
{
	struct tun_struct *tun = file->private_data;
	int ret;

	if (!tun)
		return -EBADFD;

	DBG(KERN_INFO "%s: tun_chr_fasync %d\n", tun->dev->name, on);

	if ((ret = fasync_helper(fd, file, on, &tun->fasync)) < 0)
		return ret;

	if (on) {
		ret = __f_setown(file, task_pid(current), PIDTYPE_PID, 0);
		if (ret)
			return ret;
		tun->flags |= TUN_FASYNC;
	} else
		tun->flags &= ~TUN_FASYNC;

	return 0;
}

static int tun_chr_open(struct inode *inode, struct file * file)
{
	DBG1(KERN_INFO "tunX: tun_chr_open\n");
	file->private_data = NULL;
	//cosnet_cos_register();
	return 0;
}

static int tun_chr_close(struct inode *inode, struct file *file)
{
	struct tun_struct *tun = file->private_data;

	if (!tun)
		return 0;

	DBG(KERN_INFO "%s: tun_chr_close\n", tun->dev->name);

	tun_chr_fasync(-1, file, 0);

	rtnl_lock();

	/* Detach from net device */
	file->private_data = NULL;
	/* gabep1 */
	//local_ts = NULL;
	//tun->attached = 0;

	/* Drop read queue */
	skb_queue_purge(&tun->readq);
	//cosnet_purge_queues(tun);
	//cosnet_cos_deregister();

	if (!(tun->flags & TUN_PERSIST)) {
		list_del(&tun->list);
		unregister_netdevice(tun->dev);
	}

	rtnl_unlock();

	return 0;
}

static const struct file_operations tun_fops = {
	.owner	= THIS_MODULE,
//	.llseek = no_llseek,
//	.read  = do_sync_read,
//	.aio_read  = tun_chr_aio_read,
//	.write = do_sync_write,
//	.aio_write = tun_chr_aio_write,
//	.poll	= tun_chr_poll,
	.ioctl	= tun_chr_ioctl,
	.open	= tun_chr_open,
	.release = tun_chr_close,
//	.fasync = tun_chr_fasync
};

static struct miscdevice tun_miscdev = {
	.minor = TUN_MINOR+1, //201 (see include/linux/miscdevice.h)
	.name = "cnet",
	.fops = &tun_fops,
};

/* ethtool interface */

static int tun_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	cmd->supported		= 0;
	cmd->advertising	= 0;
	cmd->speed		= SPEED_10;
	cmd->duplex		= DUPLEX_FULL;
	cmd->port		= PORT_TP;
	cmd->phy_address	= 0;
	cmd->transceiver	= XCVR_INTERNAL;
	cmd->autoneg		= AUTONEG_DISABLE;
	cmd->maxtxpkt		= 0;
	cmd->maxrxpkt		= 0;
	return 0;
}

static void tun_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	struct tun_struct *tun = netdev_priv(dev);

	strcpy(info->driver, DRV_NAME);
	strcpy(info->version, DRV_VERSION);
	strcpy(info->fw_version, "N/A");

	switch (tun->flags & TUN_TYPE_MASK) {
	case TUN_TUN_DEV:
		strcpy(info->bus_info, "cnet");
		break;
	case TUN_TAP_DEV:
		strcpy(info->bus_info, "cnet");
		break;
	}
}

static u32 tun_get_msglevel(struct net_device *dev)
{
#ifdef TUN_DEBUG
	struct tun_struct *tun = netdev_priv(dev);
	return tun->debug;
#else
	return -EOPNOTSUPP;
#endif
}

static void tun_set_msglevel(struct net_device *dev, u32 value)
{
#ifdef TUN_DEBUG
	struct tun_struct *tun = netdev_priv(dev);
	tun->debug = value;
#endif
}

static u32 tun_get_link(struct net_device *dev)
{
//	struct tun_struct *tun = netdev_priv(dev);
	return 1;//tun->attached;
}

static u32 tun_get_rx_csum(struct net_device *dev)
{
	struct tun_struct *tun = netdev_priv(dev);
	return (tun->flags & TUN_NOCHECKSUM) == 0;
}

static int tun_set_rx_csum(struct net_device *dev, u32 data)
{
	struct tun_struct *tun = netdev_priv(dev);
	if (data)
		tun->flags &= ~TUN_NOCHECKSUM;
	else
		tun->flags |= TUN_NOCHECKSUM;
	return 0;
}

static const struct ethtool_ops tun_ethtool_ops = {
	.get_settings	= tun_get_settings,
	.get_drvinfo	= tun_get_drvinfo,
	.get_msglevel	= tun_get_msglevel,
	.set_msglevel	= tun_set_msglevel,
	.get_link	= tun_get_link,
	.get_rx_csum	= tun_get_rx_csum,
	.set_rx_csum	= tun_set_rx_csum
};

static int __init tun_init(void)
{
	int ret = 0;

	printk(KERN_INFO "tun: %s, %s\n", DRV_DESCRIPTION, DRV_VERSION);
	printk(KERN_INFO "tun: %s\n", DRV_COPYRIGHT);

	ret = misc_register(&tun_miscdev);
	if (ret)
		printk(KERN_ERR "tun: Can't register misc device %d\n", TUN_MINOR);

	cosnet_cos_register();
	
	return ret;
}

static void tun_cleanup(void)
{
	struct tun_struct *tun, *nxt;

	cosnet_cos_deregister();

	misc_deregister(&tun_miscdev);

	rtnl_lock();
	list_for_each_entry_safe(tun, nxt, &tun_dev_list, list) {
		DBG(KERN_INFO "%s cleaned up\n", tun->dev->name);
		unregister_netdevice(tun->dev);
	}
	rtnl_unlock();

}

module_init(tun_init);
module_exit(tun_cleanup);
MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_AUTHOR(DRV_COPYRIGHT);
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(TUN_MINOR+1);
