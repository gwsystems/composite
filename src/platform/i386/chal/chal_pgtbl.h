#ifndef CHAL_PGTBL
#define CHAL_PGTBL

int chal_pgtbl_kmem_act(pgtbl_t pt, u32_t addr, unsigned long *kern_addr, unsigned long **pte_ret);
extern int chal_tlb_quiescence_check(u64_t timestamp);
extern int chal_pgtbl_activate(struct captbl *t, unsigned long cap, unsigned long capin, pgtbl_t pgtbl, u32_t lvl);
extern int chal_pgtbl_deactivate(struct captbl *t, struct cap_captbl *dest_ct_cap, unsigned long capin, livenessid_t lid,
                                 capid_t pgtbl_cap, capid_t cosframe_addr, const int root);

#endif
