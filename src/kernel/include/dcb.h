/**
 * Copyright 2019 by Phani Gadepalli, phanikishoreg@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General Public License v2.
 */

#ifndef DCB_H
#define DCB_H

#include "cap_ops.h"
#include "pgtbl.h"
#include "retype_tbl.h"
#include "component.h"

#define DCB_ENTRIES_MAX_PER_PAGE (PAGE_SIZE/sizeof(struct cos_dcb_info))

struct cap_dcb {
	struct cap_header     h;
	struct liveness_data  liveness;
	unsigned int          refcnt;
	vaddr_t               kern_addr;
	cpuid_t               cpuid;
} __attribute__((packed));

static int
dcb_activate(struct captbl *t, capid_t ctcap, capid_t dcbcap, capid_t ptcap, vaddr_t kaddr, livenessid_t lid, capid_t ptcapin, vaddr_t uaddr)
{
	struct cap_dcb      *dc;
	struct cap_pgtbl    *ptc;
	unsigned long       *tpte;
	struct cos_dcb_info *di;
	int                  ret;

	ret = cap_kmem_activate(t, ptcap, kaddr, (unsigned long *)&di, &tpte);
	if (unlikely(ret)) return -EINVAL;
	assert(di && tpte);

	/* TODO: memactivate kaddr -> uaddr in ptcapin */

	dc = (struct cap_dcb *)__cap_capactivate_pre(t, ctcap, dcbcap, CAP_DCB, &ret);
	if (!dc) return -EINVAL;

	ltbl_get(lid, &dc->liveness);
	dc->kern_addr = (vaddr_t)di;
	dc->refcnt    = 0;
	dc->cpuid     = get_cpuid();

	__cap_capactivate_post(&dc->h, CAP_DCB);

	return 0;
}

static int
dcb_deactivate(struct cap_captbl *ct, capid_t dcbcap, livenessid_t lid, capid_t ptcap, capid_t cosframe_addr, capid_t ptcapin, vaddr_t uaddrin)
{
	struct cap_dcb *dc;
	int ret;

	dc = (struct cap_comp *)captbl_lkup(ct->captbl, dcbcap);
	if (dc->h.type != CAP_DCB) return -EINVAL;

	if (dc->refcnt) return -EPERM;
	/* TODO: verify uaddrin in ptcapin maps to kaddr for this dcb and then unmap from ptcapin at uaddrin */

	ltbl_expire(&dc->liveness);
	ret = kmem_deact_pre(dc, ct, ptcap, cosframe_addr, &pte, &old_v);
	if (ret) return ret;
	ret = kmem_deact_post(pte, old_v);
	if (ret) return ret;
	dc->kern_addr = 0;

	return cap_capdeactivate(ct, dcbcap, CAP_DCB, lid);
}

static inline int
dcb_thd_ref(struct cap_dcb *dc, struct thread *thd)
{
	if (dc->refcnt >= DCB_ENTRIES_MAX_PER_PAGE) return -EINVAL;
	if (dc->cpuid != thd->cpuid) return -EINVAL;
	if (!ltbl_isalive(&dc->liveness)) return -EPERM;

	dc->refcnt++;

	return 0;
}

static inline int
dcb_thd_deref(struct cap_dcb *dc, struct thread *thd)
{
	if (!dc->refcnt) return -EINVAL;
	if (dc->cpuid != thd->cpuid) return -EINVAL;
	if (!ltbl_isalive(&dc->liveness)) return -EPERM;

	assert((vaddr_t)thd->dcbinfo >= dc->kern_addr && (vaddr_t)thd->dcbinfo < (dc->kern_addr + PAGE_SIZE));

	dc->refcnt--;

	return 0;
}

#endif /* DCB_H */
