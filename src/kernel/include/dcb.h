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
#include "thd.h"

#define DCB_ENTRIES_MAX_PER_PAGE (PAGE_SIZE/sizeof(struct cos_dcb_info))

struct cap_dcb {
	struct cap_header     h;
	struct liveness_data  liveness;
	unsigned int          refcnt;
	vaddr_t               kern_addr;
	cpuid_t               cpuid;
} __attribute__((packed));

static int
dcb_activate(struct captbl *t, capid_t ctcap, capid_t dcbcap, vaddr_t kaddr, livenessid_t lid, capid_t ptcapin, vaddr_t uaddr)
{
	struct cap_dcb      *dc;
	struct cap_pgtbl    *ptcin;
	int                  ret;
	paddr_t              pf = chal_va2pa((void *)kaddr);

	ptcin = (struct cap_pgtbl *)captbl_lkup(t, ptcapin);
	if (!ptcin || ptcin->h.type != CAP_PGTBL) return -EINVAL;
	/* FIXME: hard coded page order */
#if defined (__PROTECTED_DISPATCH__)
	if (pgtbl_mapping_add(ptcin->pgtbl, uaddr, pf, PGTBL_USER_DEF | ULK_PGTBL_FLAG, 12)) return -EINVAL;
#else
	if (pgtbl_mapping_add(ptcin->pgtbl, uaddr, pf, PGTBL_USER_DEF, 12)) return -EINVAL;
#endif

	dc = (struct cap_dcb *)__cap_capactivate_pre(t, ctcap, dcbcap, CAP_DCB, &ret);
	if (!dc) return -EINVAL;

	ltbl_get(lid, &dc->liveness);
	dc->kern_addr = kaddr;
	memset((void *)kaddr, 0, PAGE_SIZE);
	dc->refcnt    = 0;
	dc->cpuid     = get_cpuid();

	__cap_capactivate_post(&dc->h, CAP_DCB);

	return 0;
}

static int
dcb_deactivate(struct cap_captbl *ct, capid_t dcbcap, livenessid_t lid, capid_t ptcap, capid_t cosframe_addr, capid_t ptcapin, vaddr_t uaddrin)
{
	struct cap_dcb *dc;
	struct cap_pgtbl *ptcin;
	unsigned long *pte, addr, flags, old_v;
	int ret;

	dc = (struct cap_dcb *)captbl_lkup(ct->captbl, dcbcap);
	if (!dc || dc->h.type != CAP_DCB) return -EINVAL;

	if (!ptcapin || !uaddrin) return -EINVAL;
	ptcin = (struct cap_pgtbl *)captbl_lkup(ct->captbl, ptcapin);
	if (!ptcin || ptcin->h.type != CAP_PGTBL) return -EINVAL;
	pte = pgtbl_lkup(ptcin->pgtbl, uaddrin, (word_t *)&flags);
	if (!pte) return -EINVAL;
	if ((vaddr_t)pte != dc->kern_addr) return -EINVAL;

	if (dc->refcnt) return -EPERM;

	ltbl_expire(&dc->liveness);
	ret = kmem_deact_pre((struct cap_header *)dc, ct->captbl, ptcap, cosframe_addr, &pte, &old_v);
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

	if ((vaddr_t)thd->dcbinfo < dc->kern_addr || (vaddr_t)thd->dcbinfo > (dc->kern_addr + PAGE_SIZE)) return -EINVAL;
	if (!ltbl_isalive(&dc->liveness)) return -EPERM;

	dc->refcnt--;

	return 0;
}

#endif /* DCB_H */
