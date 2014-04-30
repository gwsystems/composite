/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef COMPONENT_H
#define COMPONENT_H

#include "liveness_tbl.h"
#include "captbl.h"

struct comp_info {
	struct liveness_data liveness;
	pgtbl_t pgtbl;
	struct captbl *captbl;
	struct cos_sched_data_area *comp_nfo;
} __attribute__((packed));

struct cap_comp {
	struct cap_header h;
	vaddr_t entry_addr;
	struct comp_info info;
} __attribute__((packed));

static int 
comp_activate(struct captbl *t, capid_t cap, capid_t capin, capid_t captbl_cap, capid_t pgtbl_cap, 
	      livenessid_t lid, vaddr_t entry_addr, struct cos_sched_data_area *sa)
{
	struct cap_comp   *compc;
	struct cap_pgtbl  *ptc;
	struct cap_captbl *ctc;
	int ret = 0;

	ctc = (struct cap_captbl *)captbl_lkup(t, captbl_cap);
	if (unlikely(!ctc || ctc->h.type != CAP_CAPTBL || ctc->lvl > 0)) return -EINVAL;
	ptc = (struct cap_pgtbl *)captbl_lkup(t, pgtbl_cap);
	if (unlikely(!ptc || ptc->h.type != CAP_PGTBL || ptc->lvl > 0)) return -EINVAL;
	
	compc = (struct cap_comp *)__cap_capactivate_pre(t, cap, capin, CAP_COMP, &ret);
	if (!compc) return ret;
	compc->entry_addr    = entry_addr;
	compc->info.pgtbl    = ptc->pgtbl;
	compc->info.captbl   = ctc->captbl;
	compc->info.comp_nfo = sa;
	ltbl_get(lid, &compc->info.liveness);
	__cap_capactivate_post(&compc->h, CAP_COMP, 0);

	return 0;
}

static int comp_deactivate(struct captbl *t, capid_t cap, capid_t capin)
{ return cap_capdeactivate(t, cap, capin, CAP_COMP); }

void comp_init(void)
{ assert(sizeof(struct cap_comp) <= __captbl_cap2bytes(CAP_COMP)); }

#endif /* COMPONENT_H */
