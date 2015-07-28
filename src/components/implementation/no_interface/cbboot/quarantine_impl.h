#ifndef QUARANTINE_IMPL_H
#define QUARANTINE_IMPL_H

#include <print.h>
#include <cos_component.h>
#include <cobj_format.h>
#include <mem_mgr.h>
#include <sched.h>
#include <cos_alloc.h>
#include <cinfo.h>
#include <cos_vect.h>
#include <cbuf.h>
#include <quarantine.h>

/* structures populated by cbboot boot_deps */
extern cos_vect_t spd_sect_cbufs;
extern cos_vect_t spd_sect_cbufs_header;

/* populated by lock.c */
extern cos_vect_t bthds;

struct cbid_caddr {
	cbuf_t cbid;
	void *caddr;
};

/* Need static storage for tracking cbufs to avoid dynamic allocation
 * before boot_deps_map_sect finishes. Each spd has probably 12 or so
 * sections, so one page of cbuf_t (1024 cbufs) should be enough to boot
 * about 80 components. This could possibly use a CSLAB? */
#define CBUFS_PER_PAGE (PAGE_SIZE / sizeof(cbuf_t))
#define SECT_CBUF_PAGES (1)
extern struct cbid_caddr all_spd_sect_cbufs[CBUFS_PER_PAGE * SECT_CBUF_PAGES];
extern unsigned int all_cbufs_index;

/* some internally-useful functions */
extern void* quarantine_translate_addr(spdid_t spdid, vaddr_t addr);
extern int lock_help_owners(spdid_t spdid, spdid_t spd);

/* prototypes for wrappers of booter.c functions used when quarantining */
int 
__boot_spd_set_symbs(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci);
int __boot_spd_caps(struct cobj_header *h, spdid_t spdid);
int __boot_spd_thd(spdid_t spdid);
void __boot_deps_save_hp(spdid_t spdid, void *hp);

#endif /* QUARANTINE_IMPL_H */
