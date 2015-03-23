
#include "quarantine_impl.h"

/* quarantine if */
spdid_t
quarantine_fork(spdid_t spdid, spdid_t source)
{
	spdid_t d_spd = 0;
	struct cbid_caddr *sect_cbufs;
	struct cobj_header *h;
	struct cobj_sect *sect;
	vaddr_t init_daddr;
	long tot = 0;
	int j;

	sect_cbufs = cos_vect_lookup(&spd_sect_cbufs, source);
	h = cos_vect_lookup(&spd_sect_cbufs_header, source);
	if (!sect_cbufs || !h) BUG(); //goto done;
	
	/* The following, copied partly from booter.c,  */
	if ((d_spd = cos_spd_cntl(COS_SPD_CREATE, 0, 0, 0)) == 0) BUG();
	sect = cobj_sect_get(h, 0);
	init_daddr = sect->vaddr;
	if (cos_spd_cntl(COS_SPD_LOCATION, d_spd, sect->vaddr, SERVICE_SIZE)) BUG();

	for (j = 0 ; j < (int)h->nsect ; j++) {
		tot += cobj_sect_size(h, j);
	}
	if (tot > SERVICE_SIZE) {
		if (cos_vas_cntl(COS_VAS_SPD_EXPAND, d_spd, sect->vaddr + SERVICE_SIZE, 
				 3 * round_up_to_pgd_page(1))) {
			printc("cbboot: could not expand VAS for component %d\n", d_spd);
			BUG();
		}
	}

	vaddr_t prev_map = 0;
	for (j = 0 ; j < (int)h->nsect ; j++) {
		vaddr_t d_addr;
		struct cbid_caddr cbm;
		cbuf_t cbid;
		int flags;
		int left;

		sect = cobj_sect_get(h, j);
		d_addr = sect->vaddr;
		cbm = sect_cbufs[j];
		cbid = cbm.cbid;

		left       = cobj_sect_size(h, j);
		/* previous section overlaps with this one, don't remap! */
		if (round_to_page(d_addr) == prev_map) {
			left -= (prev_map + PAGE_SIZE - d_addr);
			d_addr = prev_map + PAGE_SIZE;
		}
		if (left > 0) {
			left = round_up_to_page(left);
			prev_map = d_addr;

			if (sect->flags & COBJ_SECT_WRITE) flags = MAPPING_RW;
			else flags = MAPPING_READ;
			flags |= 2; /* no valloc */

			if (d_addr != (cbuf_map_at(cos_spd_id(), cbid, d_spd, d_addr | flags))) BUG();
			if (sect->flags & COBJ_SECT_CINFO) {
				/* fixup cinfo page */
				struct cos_component_information *ci = cbm.caddr;
				ci->cos_this_spd_id = d_spd;
				__boot_spd_set_symbs(h, d_spd, ci);
				__boot_deps_save_hp(d_spd, ci->cos_heap_ptr);
			}
			prev_map += left - PAGE_SIZE;
			d_addr += left;
		}

	}

	if (cos_spd_cntl(COS_SPD_ACTIVATE, d_spd, h->ncap, 0)) BUG();
	if (__boot_spd_caps(h, d_spd)) BUG();

	/* inform servers about fork */
	if (tot > SERVICE_SIZE) tot = SERVICE_SIZE + 3 * round_up_to_pgd_page(1) - tot;
	else tot = SERVICE_SIZE - tot;
	mman_fork_spd(cos_spd_id(), source, d_spd, prev_map + PAGE_SIZE, tot);

	/* deal with threads */
	if (h->flags & COBJ_INIT_THD) __boot_spd_thd(d_spd);

done:
	return d_spd;
}

