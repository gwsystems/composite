/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef INV_H
#define INV_H

#include <component.h>

/* Note: h.poly is the u16_t that is passed up to the component as spdid (in the current code) */
struct cap_sinv {
	struct cap_header h;
	struct comp_info comp_info;
	unsigned long entry_addr;
} __attribute__((packed));

struct cap_asnd {
	struct cap_header h;
	u32_t cpuid, arcv_capid, epoch; /* identify reciever */
	struct comp_info comp_info;

	/* deferrable server to rate-limit IPIs */
	u32_t budget, period, replenish_amnt;
	u64_t replenish_time; 	   /* time of last replenishment */
} __attribute__((packed));

struct cap_arcv {
	struct cap_header h;
	struct comp_info comp_info;
	u32_t pending, cpuid;
	u32_t thd_capid, thd_epoch;
} __attribute__((packed));

void inv_init(void)
{ 
	assert(sizeof(struct cap_sinv) <= __captbl_cap2bytes(CAP_SINV)); 
	assert(sizeof(struct cap_asnd) <= __captbl_cap2bytes(CAP_ASND)); 
	assert(sizeof(struct cap_arcv) <= __captbl_cap2bytes(CAP_ARCV)); 
}

#ifndef /* INV_H */
