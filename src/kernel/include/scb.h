/**
 * Copyright 2019 by Phani Gadepalli, phanikishoreg@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General Public License v2.
 */

#ifndef SCB_H
#define SCB_H

#include "component.h"
#include "cap_ops.h"
#include "pgtbl.h"
#include "retype_tbl.h"

struct comp_info;

struct cap_scb {
	struct cap_header     h;
	struct liveness_data  liveness;
	struct cap_comp      *compc;
	vaddr_t               kern_addr;
} __attribute__((packed));

static int
scb_activate(struct captbl *t, capid_t ctcap, capid_t scbcap, vaddr_t kaddr, livenessid_t lid)
{
	struct cap_scb *sc;
	int             ret;

	sc = (struct cap_scb *)__cap_capactivate_pre(t, ctcap, scbcap, CAP_SCB, &ret);
	if (!sc) return -EINVAL;

	ltbl_get(lid, &sc->liveness);
	sc->kern_addr = kaddr;
	sc->compc     = NULL;
	memset((void *)kaddr, 0, COS_SCB_SIZE);

	__cap_capactivate_post(&sc->h, CAP_SCB);

	return 0;
}

static int
scb_deactivate(struct cap_captbl *ct, capid_t scbcap, capid_t ptcap, capid_t cosframe_addr, livenessid_t lid)
{
	struct cap_scb *sc;
	unsigned long old_v = 0, *pte = NULL;
	int ret;

	sc = (struct cap_scb *)captbl_lkup(ct->captbl, scbcap);
	if (!sc || sc->h.type != CAP_SCB) return -EINVAL;

	/* FIXME: component using this scbcap is still active! how to handle this? */
	if (sc->compc) return -EPERM;

	ltbl_expire(&sc->liveness);
	ret = kmem_deact_pre((struct cap_header *)sc, ct->captbl, ptcap, cosframe_addr, &pte, &old_v);
	if (ret) return ret;
	ret = kmem_deact_post(pte, old_v);
	if (ret) return ret;

	return cap_capdeactivate(ct, scbcap, CAP_SCB, lid);
}

static inline int
scb_comp_update(struct captbl *ct, struct cap_scb *sc, struct cap_comp *compc)
{
	//paddr_t pf = chal_va2pa((void *)(sc->kern_addr));

	if (unlikely(!ltbl_isalive(&sc->liveness))) return -EPERM;
	/* FIXME: hard coded pgtbl order */
	//if (pgtbl_mapping_add(ptcin->pgtbl, uaddrin, pf, PGTBL_USER_DEF, 12)) return -EINVAL;

	//sc->compc = compc;
	compc->info.scb_data = (struct cos_scb_info *)(sc->kern_addr);
	return 0;
}

static inline int
scb_mapping(struct captbl *ct, struct cap_scb *sc, struct cap_pgtbl *ptcin, struct cap_comp *compc, vaddr_t uaddrin)
{
	assert(sc->compc == compc || !sc->compc);
	paddr_t pf = chal_va2pa((void *)(sc->kern_addr));

	if (pgtbl_mapping_add(ptcin->pgtbl, uaddrin, pf, PGTBL_USER_DEF, 12)) return -EINVAL;

	return 0;
}

static inline int
scb_comp_remove(struct cap_captbl *ct, struct cap_scb *sc, capid_t ptcapin, vaddr_t uaddrin)
{
	int ret;

	if (unlikely(!ct || !sc || !ptcapin || !uaddrin)) return -EINVAL;

	if (unlikely(!ltbl_isalive(&sc->liveness))) return -EPERM;
	if (unlikely(!sc->compc)) return -EINVAL;

	/* TODO: unmap uaddrin in the user-land */

	return 0;
}

static inline struct liveness_data *
scb_liveness(struct cap_scb *sc)
{
	return &sc->liveness;
}

#endif /* SCB_H */
