#include "include/shared/cos_types.h"
#include "include/captbl.h"
#include "include/pgtbl.h"
#include "include/cap_ops.h"
#include "include/liveness_tbl.h"
#include "include/retype_tbl.h"
#include "chal/chal_pgtbl.h"

int
pgtbl_kmem_act(pgtbl_t pt, u32_t addr, unsigned long *kern_addr, unsigned long **pte_ret)
{
	return chal_pgtbl_kmem_act(pt, addr, kern_addr, pte_ret);
}

/* Return 1 if quiescent past since input timestamp. 0 if not. */
int
tlb_quiescence_check(u64_t timestamp)
{
	return chal_tlb_quiescence_check(timestamp);
}

int
cap_memactivate(struct captbl *ct, struct cap_pgtbl *pt, capid_t frame_cap, capid_t dest_pt, vaddr_t vaddr)
{
	return chal_cap_memactivate(ct, pt, frame_cap, dest_pt, vaddr);
}

int
pgtbl_activate(struct captbl *t, unsigned long cap, unsigned long capin, pgtbl_t pgtbl, u32_t lvl)
{
	return chal_pgtbl_activate(t, cap, capin, pgtbl, lvl);
}

int
pgtbl_deactivate(struct captbl *t, struct cap_captbl *dest_ct_cap, unsigned long capin, livenessid_t lid,
		 capid_t pgtbl_cap, capid_t cosframe_addr, const int root)
{
	return chal_pgtbl_deactivate(t, dest_ct_cap, capin, lid, pgtbl_cap, cosframe_addr, root);
}
