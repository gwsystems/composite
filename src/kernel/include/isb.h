#ifndef ISB_H
#define ISB_H 

#include "component.h"
#include "cap_ops.h"
#include "pgtbl.h"

struct cap_isb {
    struct cap_header     h;
    struct liveness_data  liveness;
    size_t                npages;
} __attribute__((packed));


// static int
// isb_activate(struct captbl *t, capid_t ctcap, capid_t isbcap, size_t npages, livenessid_t lid)
// {
//     struct cap_isb *cisb;
//     int             ret;

//     cisb = (struct cap_isb *)__cap_capactivate_pre(t, ctcap, isbcap, CAP_ISB, &ret);
//     if (!cisb) return -EINVAL;

//     ltbl_get(lid, &cisb->liveness);
//     cisb->npages = npages;
//     memset((void *)ULK_BASE_ADDR, 0, PAGE_SIZE * npages);
//     cos_isb_alloced = 1;

//     __cap_capactivate_post(&cisb->h, CAP_ISB);

//     return 0;
// }

// static int
// isb_deactivate(struct cap_captbl *ct, capid_t isbcap, capid_t ptcap, capid_t cosframe_addr, livenessid_t lid)
// {
//     /* TODO */
//     assert(0);
//     return 0;
// }

// static inline int
// isb_mapin(struct captbl *ct, struct cap_isb *cisb, struct cap_pgtbl *ptcin, vaddr_t uaddr)
// {
//     if (unlikely(!ltbl_isalive(&cisb->liveness))) return -EPERM;
// 	paddr_t pa = chal_va2pa((void *)(cisb->k_addr));

// 	if (pgtbl_mapping_add(ptcin->pgtbl, uaddr, pa, PGTBL_USER_DEF | (1ul << 59), 12)) return -EINVAL;

// 	return 0;
// }

#endif