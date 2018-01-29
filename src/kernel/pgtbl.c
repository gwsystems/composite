/******************************************************************************
Filename    : pgtbl.c
Author      : Gabriel Parmer
Date        : ?/?/2015
Licence     : GPL v2; see COPYING for details.
Description : The page table operations that does not need special optimizations.
******************************************************************************/

/* Includes ******************************************************************/
#include "include/shared/cos_types.h"
#include "include/captbl.h"
#include "include/pgtbl.h"
#include "include/cap_ops.h"
#include "include/liveness_tbl.h"
#include "include/retype_tbl.h"
#include "chal/chal_prot.h"
/* End Includes **************************************************************/

/* Begin Function:pgtbl_kmem_act **********************************************
Description : Activate a page table entry as a kernel page.
Input       : pgtbl_t pt - The page table structure.
              u32_t addr - The virtual address of the memory to activate.
Output      : unsigned long *kern_addr - The kernel address of the memory page.
              unsigned long **pte_ret - The pointer to the PTE, so that we can 
                                        release the page if the kernel object 
                                        activation failed later.
Return      : int - If successful, 0; or an error code.
******************************************************************************/
int
pgtbl_kmem_act(pgtbl_t pt, u32_t addr, unsigned long *kern_addr, unsigned long **pte_ret)
{
	return chal_pgtbl_kmem_act(pt, addr, kern_addr, pte_ret);
}
/* End Function:pgtbl_kmem_act ***********************************************/

/* Begin Function:tlb_quiescence_check ****************************************
Description : Check if the TLB is quiescent. This function will return 1 if 
              the quiescence time have passed since input timestamp. 0 if not.
Input       : u64_t timestamp - The reference timestamp.
Output      : None.
Return      : int - If quiescent, 0; else 1.
******************************************************************************/
int
tlb_quiescence_check(u64_t timestamp)
{
	return chal_tlb_quiescence_check(timestamp);
}
/* End Function:tlb_quiescence_check *****************************************/

/* Begin Function:cap_memactivate *********************************************
Description : Delegate a user page from one page table to another. This will be 
              called by the capinv.c's system call path.
Input       : struct captbl *ct - The capability table for the destination page table.
              struct cap_pgtbl *pt - The source page table.
              capid_t frame_cap - The virtual address of the cosframe to delegate.
              vaddr_t vaddr - The virtual address of to delegate to on the destination page table.
Output      : None.
Return      : int  - If successful, 0; else an error code.
******************************************************************************/
int
cap_memactivate(struct captbl *ct, struct cap_pgtbl *pt, capid_t frame_cap, capid_t dest_pt, vaddr_t vaddr)
{
	return chal_cap_memactivate(ct, pt, frame_cap, dest_pt, vaddr);
}
/* End Function:cap_memactivate **********************************************/

/* Begin Function:pgtbl_activate **********************************************
Description : Activate(create) a page table capability in the capability table
              designated. At this point, the page table data structure page is 
              already allocated.
Input       : struct captbl *t - The master capability table.
              unsigned long cap - The capability to the capability table that you want 
                                  this newly created page table capability to be in.
              unsigned long capin - The position that you want this page table capability
                                    to be in.
              pgtbl_t pgtbl - The page table data structure.
              u32_t lvl - The level of this page table.
Output      : None.
Return      : int  - If successful, 0; else an error code.
TODO        : 1. The captbl is named t instead of ct. Inconsistency.
              2. cap and capin is badly named.
              3. u32_t, fixed length types are not generic.
******************************************************************************/
int
pgtbl_activate(struct captbl *t, unsigned long cap, unsigned long capin, pgtbl_t pgtbl, u32_t lvl)
{
	return chal_pgtbl_activate(t, cap, capin, pgtbl, lvl);
}
/* End Function:pgtbl_activate ***********************************************/

/* Begin Function:pgtbl_deactivate ********************************************
Description : Deactivate a page table capability.
Input       : struct captbl *t - The master capability table.
              struct cap_captbl *dest_ct_cap - The capability to the target 
                                               capability table.
              unsigned long capin - The capid of the page table capability in the
                                    target capability table.
              livenessid_t lid - The liveness id of the XXX.
              capid_t pgtbl_cap - The capid of the page table that contains the
                                  mapping of the memory trunk used by the page table
                                  that is to be freed. If we free the target page table,
                                  we are freeing a page; thus we need the capability
                                  to the page table that contains the page to be freed
                                  to do operations on it.
              capid_t cosframe_addr - The address of the cosframe used by the target
                                      page table. This is only needed when the page table
                                      is freed.
              const int root - Whether we are doing a root deletion. If not root we are
                               deactivating just a alias, if root then we are deactivating
                               the kernel object as well.
Output      : None.
Return      : int  - If successful, 0; else an error code.
TODO        : 1. The dest_ct_cap is badly named, consider changing to target_ct.
              2. captbl is named t instead of ct. Inconsistency.
              3. capin is badly named.
              4. Change livenessid_t to lid_t. The name is too long!
              5. This function is too complex. The root argument should be deleted 
                 and the functionality should be moved to a separate function.
******************************************************************************/
int
pgtbl_deactivate(struct captbl *t, struct cap_captbl *dest_ct_cap, unsigned long capin,
                 livenessid_t lid, capid_t pgtbl_cap, capid_t cosframe_addr, const int root)
{
	return chal_pgtbl_deactivate(t, dest_ct_cap, capin, lid, pgtbl_cap, cosframe_addr, root);
}
/* End Function:pgtbl_deactivate *********************************************/

/*
 * this works on both kmem and regular user memory: the retypetbl_ref
 * works on both.
 */
int
pgtbl_mapping_add(pgtbl_t pt, u32_t addr, u32_t page, u32_t flags)
{
	return chal_pgtbl_mapping_add(pt, addr, page, flags);
}

/* This function is only used by the booting code to add cos frames to
 * the pgtbl. It ignores the retype tbl (as we are adding untyped
 * frames). */
int
pgtbl_cosframe_add(pgtbl_t pt, u32_t addr, u32_t page, u32_t flags)
{
	return chal_pgtbl_cosframe_add(pt, addr, page, flags);
}

/* This function updates flags of an existing mapping. */
int
pgtbl_mapping_mod(pgtbl_t pt, u32_t addr, u32_t flags, u32_t *prevflags)
{
	return chal_pgtbl_mapping_mod(pt, addr, flags, prevflags);
}

/* When we remove a mapping, we need to link the vas to a liv_id,
 * which tracks quiescence for us. */
int
pgtbl_mapping_del(pgtbl_t pt, u32_t addr, u32_t liv_id)
{
	return chal_pgtbl_mapping_del(pt, addr, liv_id);
}

/* NOTE: This just removes the mapping. NO liveness tracking! TLB
 * flush should be taken care of separately (and carefully). */
int
pgtbl_mapping_del_direct(pgtbl_t pt, u32_t addr)
{
	return chal_pgtbl_mapping_del_direct(pt, addr);
}

int
pgtbl_mapping_scan(struct cap_pgtbl *pt)
{
	return chal_pgtbl_mapping_scan(pt);
}

void *
pgtbl_lkup_lvl(pgtbl_t pt, u32_t addr, u32_t *flags, u32_t start_lvl, u32_t end_lvl)
{
	return chal_pgtbl_lkup_lvl(pt, addr, flags, start_lvl, end_lvl);
}

int
pgtbl_ispresent(u32_t flags)
{
	return chal_pgtbl_ispresent(flags);
}

unsigned long *
pgtbl_lkup(pgtbl_t pt, u32_t addr, u32_t *flags)
{
	return chal_pgtbl_lkup(pt, addr, flags);
}

unsigned long *
pgtbl_lkup_pte(pgtbl_t pt, u32_t addr, u32_t *flags)
{
	return chal_pgtbl_lkup_pte(pt, addr, flags);
}

int
pgtbl_get_cosframe(pgtbl_t pt, vaddr_t frame_addr, paddr_t *cosframe)
{
	return chal_pgtbl_get_cosframe(pt, frame_addr, cosframe);
}

/* vaddr -> kaddr */
vaddr_t
pgtbl_translate(pgtbl_t pt, u32_t addr, u32_t *flags)
{
	return (vaddr_t)pgtbl_lkup(pt, addr, flags);
}

pgtbl_t
pgtbl_create(void *page, void *curr_pgtbl)
{
	return chal_pgtbl_create(page, curr_pgtbl);
}

int
pgtbl_quie_check(u32_t orig_v)
{
	return chal_pgtbl_quie_check(orig_v);
}

void
pgtbl_init_pte(void *pte)
{
	return chal_pgtbl_init_pte(pte);
}

/* End Of File ***************************************************************/

/* Copyright (C) GWU Systems & Security Lab. All rights reserved *************/

