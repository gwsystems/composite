#ifndef ISB_H
#define ISB_H 

#include "component.h"
#include "cap_ops.h"
#include "pgtbl.h"

struct cap_isb {
    struct cap_header     h;
    struct liveness_data  liveness;
    vaddr_t               kern_addr;
} __attribute__((packed));

static int
isb_activate(struct captbl *t, capid_t ctcap, capid_t isbcapid, livenessid_t lid, vaddr_t kaddr, capid_t ptcid, vaddr_t uaddr)
{
    struct cap_isb   *isbcap;
    struct cap_pgtbl *ptcap;
    paddr_t           pframe = chal_va2pa((void *)kaddr);
    int               ret;

    ptcap = (struct cap_pgtbl *)captbl_lkup(t, ptcid);
    if (!ptcap || ptcap->h.type != CAP_PGTBL) return -EINVAL;
    if (pgtbl_mapping_add(ptcap->pgtbl, uaddr, pframe, PGTBL_USER_DEF, 12)) return -EINVAL;

    isbcap = (struct cap_isb *)__cap_capactivate_pre(t, ctcap, isbcapid, CAP_ISB, &ret);
    if (!isbcap) return -EINVAL;

    ltbl_get(lid, &isbcap->liveness);
    isbcap->kern_addr = kaddr;
    memset((void *)kaddr, 0, PAGE_SIZE);

    __cap_capactivate_post(&isbcap->h, CAP_ISB);

    return ret;
}

static int
isb_deactivate(struct cap_captbl *ct, capid_t isbcap, capid_t ptcap, capid_t cosframe_addr, livenessid_t lid)
{
    /* TODO */
    assert(0);
    return 0;
}

#endif