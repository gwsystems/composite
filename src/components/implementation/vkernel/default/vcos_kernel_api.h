#ifndef VCOS_KERNEL_API_H
#define VCOS_KERNEL_API_H

/*
 * Copyright 2015, Qi Wang and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_component.h>
#include <cos_debug.h>
#include <cos_kernel_api.h>

/*
 * This uses the next three functions to allocate a new component and
 * correctly populate ci (allocating all resources from ci_resources).
 */
int vcos_compinfo_alloc(struct cos_compinfo *ci, vaddr_t heap_ptr, vaddr_t entry, struct cos_compinfo *ci_resources);
captblcap_t vcos_captbl_alloc(struct cos_compinfo *ci);
pgtblcap_t vcos_pgtbl_alloc(struct cos_compinfo *ci);
compcap_t vcos_comp_alloc(struct cos_compinfo *ci, captblcap_t ctc, pgtblcap_t ptc, vaddr_t entry);

thdcap_t  vcos_thd_alloc(struct cos_compinfo *ci, compcap_t comp, cos_thd_fn_t fn, void *data);
/* Create the initial (cos_init) thread */
thdcap_t  vcos_initthd_alloc(struct cos_compinfo *ci, compcap_t comp);
sinvcap_t vcos_sinv_alloc(struct cos_compinfo *srcci, compcap_t dstcomp, vaddr_t entry);
arcvcap_t vcos_arcv_alloc(struct cos_compinfo *ci, thdcap_t thdcap, tcap_t tcapcap, compcap_t compcap, arcvcap_t enotif);
asndcap_t vcos_asnd_alloc(struct cos_compinfo *ci, arcvcap_t arcvcap, captblcap_t ctcap);
hwcap_t vcos_hw_alloc(struct cos_compinfo *ci, u32_t bitmap);

void *vcos_page_bump_alloc(struct cos_compinfo *ci);
/* TODO: TCaps! */
tcap_t vcos_tcap_split(struct cos_compinfo *ci, tcap_t src, int pool);

#endif /* VCOS_KERNEL_API_H */

