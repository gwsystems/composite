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
	vaddr_t entry_addr;
} __attribute__((packed));

struct cap_asnd {
	struct cap_header h;
	u32_t cpuid;
	u32_t arcv_cpuid, arcv_capid, epoch; /* identify reciever */
	struct comp_info comp_info;

	/* deferrable server to rate-limit IPIs */
	u32_t budget, period, replenish_amnt;
	u64_t replenish_time; 	   /* time of last replenishment */
} __attribute__((packed));

struct cap_arcv {
	struct cap_header h;
	struct comp_info comp_info;
	u32_t pending, cpuid, epoch;
	u32_t thd_capid, thd_epoch;
} __attribute__((packed));

static int 
sinv_activate(struct captbl *t, capid_t cap, capid_t capin, capid_t comp_cap, vaddr_t entry_addr)
{
	struct cap_sinv *sinvc;
	struct cap_comp *compc;
	int ret;

	compc = (struct cap_comp *)captbl_lkup(t, comp_cap);
	if (unlikely(!compc || compc->h.type != CAP_COMP)) return -EINVAL;
	
	sinvc = (struct cap_sinv *)__cap_capactivate_pre(t, cap, capin, CAP_SINV, &ret);
	if (!sinvc) return ret;
	memcpy(&sinvc->comp_info, &compc->info, sizeof(struct comp_info));
	sinvc->entry_addr = entry_addr;
	__cap_capactivate_post(sinvc, CAP_SINV, compc->h.poly);
}

static int sinv_deactivate(struct captbl *t, capid_t cap, capid_t capin)
{ return cap_capdeactivate(t, cap, capin, CAP_SINV); }

static int
asnd_activate(struct captbl *t, capid_t cap, capid_t capin, capid_t comp_cap, capid_t rcv_cap, u32_t budget, u32_t period)
{
	struct cap_asnd *asndc;
	struct cap_comp *compc;
	struct cap_arcv *arcvc;
	int ret;

	compc = (struct cap_comp *)captbl_lkup(t, comp_cap);
	if (unlikely(!compc || compc->h.type != CAP_COMP)) return -EINVAL;
	arcvc = (struct cap_arcv *)captbl_lkup(t, rcv_cap);
	if (unlikely(!arcvc || arcvc->h.type != CAP_ARCV)) return -EINVAL;
	
	asndc = (struct cap_asnd *)__cap_capactivate_pre(t, cap, capin, CAP_ASND, &ret);
	if (!asndc) return ret;
	memcpy(&asndc->comp_info, &compc->info, sizeof(struct comp_info));
	asndc->arcv_epoch     = arcvc->epoch;
	asndc->arcv_cpuid     = arcvc->cpuid;
	asndc->arcv_capid     = rcv_cap;
	asndc->period         = period;
	asndc->budget         = budget;
	asndc->replenish_amnt = budget;
	rdtscll(asndc->replenish_time);
	__cap_capactivate_post(asndc, CAP_ASND, 0);
}

static int asnd_deactivate(struct captbl *t, capid_t cap, capid_t capin)
{ return cap_capdeactivate(t, cap, capin, CAP_ASND); }

static int
arcv_activate(struct captbl *t, capid_t cap, capid_t capin, capid_t comp_cap)
{
	struct cap_comp *compc;
	struct cap_arcv *arcvc;
	int ret;

	compc = (struct cap_comp *)captbl_lkup(t, comp_cap);
	if (unlikely(!compc || compc->h.type != CAP_COMP)) return -EINVAL;

	arcvc = (struct cap_arcv *)__cap_capactivate_pre(t, cap, capin, CAP_ARCV, &ret);
	if (!arcvc) return ret;
	memcpy(&arcvc->comp_info, &compc->info, sizeof(struct comp_info));
	arcvc->pending = 0;
	arcvc->cpuid   = 0; 	/* FIXME: get the real cpuid */
	arcvc->epoch   = 0; 	/* FIXME: get the real epoch */
	arcvc->thd     = NULL;
	__cap_capactivate_post(arcvc, CAP_ARCV, 0);
}

static int arcv_deactivate(struct captbl *t, capid_t cap, capid_t capin)
{ return cap_capdeactivate(t, cap, capin, CAP_ARCV); }

void inv_init(void)
{ 
	assert(sizeof(struct cap_sinv) <= __captbl_cap2bytes(CAP_SINV)); 
	assert(sizeof(struct cap_asnd) <= __captbl_cap2bytes(CAP_ASND)); 
	assert(sizeof(struct cap_arcv) <= __captbl_cap2bytes(CAP_ARCV)); 
}

#ifndef /* INV_H */
