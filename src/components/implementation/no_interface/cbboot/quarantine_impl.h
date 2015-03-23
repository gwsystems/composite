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

extern cos_vect_t spd_sect_cbufs;
extern cos_vect_t spd_sect_cbufs_header;

struct cbid_caddr {
	cbuf_t cbid;
	void *caddr;
};

/* prototypes for wrappers of booter.c functions used when quarantining */
int 
__boot_spd_set_symbs(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci);
int __boot_spd_caps(struct cobj_header *h, spdid_t spdid);
int __boot_spd_thd(spdid_t spdid);
void __boot_deps_save_hp(spdid_t spdid, void *hp);

#endif /* QUARANTINE_IMPL_H */
