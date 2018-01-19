/******************************************************************************
Filename    : chal_prot.h
Author      : Runyu Pan
Date        : ?/?/2018
Licence     : GPL v2; see COPYING for details.
Description : The Composite chal general prototype interface.
******************************************************************************/

/* Begin Function Prototypes *************************************************/
/* Page table related prototypes */
int chal_pgtbl_kmem_act(pgtbl_t pt, u32_t addr, unsigned long *kern_addr, unsigned long **pte_ret);
int chal_tlb_quiescence_check(u64_t timestamp);
int chal_cap_memactivate(struct captbl *ct, struct cap_pgtbl *pt, capid_t frame_cap, capid_t dest_pt, vaddr_t vaddr);
int chal_pgtbl_activate(struct captbl *t, unsigned long cap, unsigned long capin, pgtbl_t pgtbl, u32_t lvl);
int chal_pgtbl_deactivate(struct captbl *t, struct cap_captbl *dest_ct_cap, unsigned long capin,
                          livenessid_t lid, capid_t pgtbl_cap, capid_t cosframe_addr, const int root);
/* End Function Prototypes ***************************************************/

/* End Of File ***************************************************************/

/* Copyright (C) GWU Systems & Security Lab. All rights reserved *************/

