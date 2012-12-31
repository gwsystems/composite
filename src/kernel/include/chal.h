/**
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Copyright The George Washington University, Gabriel Parmer,
 * gparmer@gwu.edu, 2012
 */

/* 
 * The Composite Hardware Abstraction Layer, or Hijack Abstraction
 * Layer (cHAL) is the layer that defines the platform-specific
 * functionality that requires specific implementations not only for
 * different architectures (e.g. x86-32 vs. -64), but also when
 * booting from the bare-metal versus using the Hijack techniques.
 * This file documents the functions that must be implemented within
 * the platform code, and how they interact with the _Composite_
 * kernel proper.
 */

#ifndef CHAL_H
#define CHAL_H

#include "shared/cos_types.h"

/* 
 * Namespacing in the cHAL: chal_<family>_<operation>(...).  <family>
 * is the family of operations such as pgtbl or addr operations, and
 * <operation> is the operation to perform on that family of
 * manipulations.
 */

/*************************************
 * Platform page-table manipulations *
 *************************************/

/* 
 * Switch to the specified page-tables.  This will not only switch the
 * loaded page tables on the current cpu, but also any backing
 * data-structures that are tracked in the platform code.
 * 
 * This function must be specified in the chal_plat.h file.
 */
static inline void chal_pgtbl_switch(paddr_t pt);
/* 
 * Switch any backing data-structures for the "current" page-table,
 * but _not_ the actual loaded page-tables.
 * 
 * This function must be specified in the chal_plat.h file.
 */
static inline void __chal_pgtbl_switch(paddr_t pt);

/* Add a page to pgtbl at address. 0 on success */
int chal_pgtbl_add(paddr_t pgtbl, vaddr_t vaddr, paddr_t paddr);
/* Translate a vaddr to an addressable address via pgtbl */
vaddr_t chal_pgtbl_vaddr2kaddr(paddr_t pgtbl, unsigned long addr);
/* Remove mapping for a vaddr from pgtbl. != 0 if mapping doesn't exist */
paddr_t chal_pgtbl_rem(paddr_t pgtbl, vaddr_t va);
/* unsigned long chal_pgtbl_lookup(paddr_t pgtbl, unsigned long addr); */
/* void chal_pgtbl_or_pgd(paddr_t pgtbl, unsigned long addr, unsigned long val); */
/* void chal_pgtbl_print_path(paddr_t pgtbl, unsigned long addr); */
/* void chal_pgtbl_copy_range(paddr_t pt_to, paddr_t pt_from,  */
/* 			   unsigned long lower_addr, unsigned long size); */
/* int chal_pgtbl_can_switch(void); */

/* int switch_thread_data_page(int old_thd, int new_thd); */
/* int host_attempt_brand(struct thread *brand); */
/* static const struct cos_trans_fns *trans_fns = NULL; */
/* int user_struct_fits_on_page(unsigned long addr, unsigned int size); */
/* int host_attempt_brand(struct thread *brand); */
/* void host_idle(void); */
void *va_to_pa(void *va);
void *pa_to_va(void *pa);

void *cos_alloc_page(void);
void cos_free_page(void *page);

/* int cos_syscall_idle(void); */
/* int cos_syscall_switch_thread(void); */
/* void cos_syscall_brand_wait(int spd_id, unsigned short int bid, int *preempt); */
/* void cos_syscall_brand_upcall(int spd_id, int thread_id_flags); */
/* int cos_syscall_buff_mgmt(void); */
/* void cos_syscall_upcall(void); */

#include "../../platform/include/chal_plat.h"

#endif	/* CHAL_H */
