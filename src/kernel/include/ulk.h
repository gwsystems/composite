#ifndef ULK_H
#define ULK_H 

#include "component.h"
#include "cap_ops.h"
#include "pgtbl.h"

#define ULK_PGTBL_FLAG (1ul << 59)

struct cap_ulk {
    struct cap_header     h;
    struct liveness_data  liveness;
    vaddr_t               kern_addr;
} __attribute__((packed));

static int
ulk_activate(struct captbl *t, capid_t ctcap, capid_t ulkcapid, livenessid_t lid, vaddr_t kaddr, capid_t ptcid, vaddr_t uaddr)
{
    struct cap_ulk   *ulkcap;
    struct cap_pgtbl *ptcap;
    paddr_t           pframe = chal_va2pa((void *)kaddr);
    int               ret;

    ptcap = (struct cap_pgtbl *)captbl_lkup(t, ptcid);
    if (!ptcap || ptcap->h.type != CAP_PGTBL) return -EINVAL;
    if (pgtbl_mapping_add(ptcap->pgtbl, uaddr, pframe, PGTBL_USER_DEF | ULK_PGTBL_FLAG, 12)) return -EINVAL;

    ulkcap = (struct cap_ulk *)__cap_capactivate_pre(t, ctcap, ulkcapid, CAP_ULK, &ret);
    if (!ulkcap) return -EINVAL;

    ltbl_get(lid, &ulkcap->liveness);
    ulkcap->kern_addr = kaddr;
    memset((void *)kaddr, 0, PAGE_SIZE);

    __cap_capactivate_post(&ulkcap->h, CAP_ULK);

    return ret;
}

static int
ulk_deactivate(struct cap_captbl *ct, capid_t ulkcap, capid_t ptcap, capid_t cosframe_addr, livenessid_t lid)
{
    /* TODO */
    assert(0);
    return 0;
}

#endif