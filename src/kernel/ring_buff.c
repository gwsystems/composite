/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include "include/debug.h"
#include "include/thread.h"
#include "include/chal.h"

extern vaddr_t chal_pgtbl_vaddr2kaddr(paddr_t pgtbl, unsigned long addr);
extern int user_struct_fits_on_page(unsigned long addr, unsigned int size);
/* 
 * Return -1 if there is some form of error (couldn't find ring
 * buffer, or buffer is of inappropriate size), 1 if there is no
 * error, but there is no available buffer, 0 if found_buff and
 * found_len are set to the buffer's values.
 *
 * See cos_types.h for a description of the structure of the buffer.
 */
int rb_retrieve_buff(struct cos_net_acap_info *net_acap, int desired_len, 
		     void **found_buf, int *found_len)
{
	ring_buff_t *rb;
	int position;
	void *addr;
	vaddr_t kaddr;
	unsigned short int status, len;
	struct spd *bspd;
	
	assert(net_acap);
	rb = net_acap->k_rb;
	if (!rb) {
		return -1;
	}
	position = net_acap->rb_next;
	addr     = rb->packets[position].ptr;
	len      = rb->packets[position].len;
	status   = rb->packets[position].status;
	if (RB_READY != status || NULL == addr) {
		return 1;
	}
	if (len < desired_len) {
		printk("ring_buff: length of user buffer not sufficient.\n");
		return -1;
	}
	assert(net_acap->acap);
	bspd = spd_get_by_index(net_acap->acap->srv_spd_id);
	assert(bspd);
	kaddr = chal_pgtbl_vaddr2kaddr(bspd->spd_info.pg_tbl, (unsigned long)addr);
	if (!kaddr || !user_struct_fits_on_page(kaddr, len)) {
		rb->packets[position].status = RB_ERR;
		net_acap->rb_next = (position+1) & (RB_SIZE-1);
		printk("ring_buff: user buffer either not in memory, or not page_aligned.\n");
		return -1;
	}

	assert(kaddr);
//	printk("kaddr to copy to is %x from ptr %x\n", kaddr, addr);
	*found_buf = (void*)kaddr;
	*found_len = len;
	
	rb->packets[position].status = RB_USED;
	net_acap->rb_next = (position+1) & (RB_SIZE-1);

	return 0;
}

/* Pass in the net_info struct who's rinb buffer is to get setup, and the user
 * and kernel pointers to it */
int rb_setup(struct cos_net_acap_info *net_acap, ring_buff_t *user_rb, ring_buff_t *kern_rb)
{
	assert(net_acap && user_rb && kern_rb);
	net_acap->u_rb = user_rb;
	net_acap->k_rb = kern_rb;
	net_acap->rb_next = 0;
	
	return 0;
}
