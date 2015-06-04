
#include "quarantine_impl.h"

#define printl(...) printc("Quarantine: "__VA_ARGS__)

/* Called after fork() finished copying the spd from source to target.
 */
int
quarantine_migrate(spdid_t spdid, spdid_t source, spdid_t target, thdid_t thread)
{

	printl("Migrate thread %d in spd %d to %d\n", thread, source, target);
	if (!thread) goto done;

	sched_quarantine_thread(cos_spd_id(), source, target, thread);

	printl("thread %d in spd %d\n", thread, cos_spd_id());

done:
	return 0;
}

/* Increment all of the (non fault handler) caps fork.cnt.
 * GB: unused and might be removed, along with COS_CAP_INC_FORK_CNT */
static int
quarantine_spd_caps(struct cobj_header *h, spdid_t spdid)
{
	struct cobj_cap *cap;
	unsigned int i;
	for (i = 0 ; i < h->ncap ; i++) {
		cap = cobj_cap_get(h, i);
		if (cobj_cap_undef(cap)) break;
		/* ignore fault handlers */
		if (cap->fault_num < COS_FLT_MAX) continue;
		if (cos_cap_cntl(COS_CAP_INC_FORK_CNT, spdid, cap->cap_off, 1)) BUG();
	}
	return 0;
}


/* quarantine if */
spdid_t
quarantine_fork(spdid_t spdid, spdid_t source)
{
	spdid_t d_spd = 0;
	thdid_t d_thd;
	struct cbid_caddr *old_sect_cbufs, *new_sect_cbufs;
	struct cobj_header *h;
	struct cobj_sect *sect;
	vaddr_t init_daddr;
	long tot = 0;
	int j, r;
	int generation;

	printl("quarantine_fork(%d, %d)\n", spdid, source);

	if (!(old_sect_cbufs = cos_vect_lookup(&spd_sect_cbufs, source)))
		printl("cos_vect_lookup(%d) in spd_sec_cbufs failed\n", source);
	if (!(h = cos_vect_lookup(&spd_sect_cbufs_header, source)))
		printl("cos_vect_lookup(%d) in spd_sec_cbufs_header failed\n", source);
	if (!old_sect_cbufs || !h) BUG(); //goto done;
	
	/* The following, copied partly from booter.c,  */
	if ((d_spd = cos_spd_cntl(COS_SPD_CREATE, 0, 0, 0)) == 0) BUG();

	printl("Created new spd: %d\n", d_spd);

	sect = cobj_sect_get(h, 0);
	init_daddr = sect->vaddr;
	if (cos_spd_cntl(COS_SPD_LOCATION, d_spd, sect->vaddr, SERVICE_SIZE)) BUG();
	printl("Set location to %x\n", (unsigned long)sect->vaddr);

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

	new_sect_cbufs = cos_vect_lookup(&spd_sect_cbufs, d_spd);
	if (!new_sect_cbufs) {
		new_sect_cbufs = &all_spd_sect_cbufs[all_cbufs_index];
		all_cbufs_index += h->nsect;
		if (cos_vect_add_id(&spd_sect_cbufs, new_sect_cbufs, d_spd) < 0) BUG();
		if (cos_vect_add_id(&spd_sect_cbufs_header, h, d_spd) < 0) BUG();
		printl("Added %d to sect_cbufs\n", d_spd);
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

		left       = cobj_sect_size(h, j);
		/* previous section overlaps with this one, don't remap! */
		if (round_to_page(d_addr) == prev_map) {
			left -= (prev_map + PAGE_SIZE - d_addr);
			d_addr = prev_map + PAGE_SIZE;
		}
		printl("Mapping section %d @ %x with %d bytes\n", j, (unsigned long)d_addr, left);
		if (left > 0) {
			left = round_up_to_page(left);
			prev_map = d_addr;

			if (sect->flags & COBJ_SECT_WRITE) flags = MAPPING_RW;
			else flags = MAPPING_READ;
			flags |= 2; /* no valloc */

			cbm = old_sect_cbufs[j];
			/* if RW, don't want to share, need new mem */
			if (flags & MAPPING_RW) {
				struct cbid_caddr new_cbm;
				new_cbm.caddr = cbuf_alloc_ext(left, &new_cbm.cbid, CBUF_EXACTSZ);
				printl("Avoid sharing, allocated new cbuf %d @ %x\n", new_cbm.cbid, (unsigned long)new_cbm.caddr);
				assert(new_cbm.caddr);
				memcpy(new_cbm.caddr, cbm.caddr, left);
				cbm.caddr = new_cbm.caddr;
				cbm.cbid = new_cbm.cbid;
			}
			assert(cbm.caddr);
			cbid = cbm.cbid;
			new_sect_cbufs[j] = cbm;

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

	/* FIXME: set fault handlers and re-write caps */
	printl("Activating %d\n", d_spd);
	if (cos_spd_cntl(COS_SPD_ACTIVATE, d_spd, h->ncap, 0)) BUG();
	printl("Setting capabilities for %d\n", d_spd);
	if (__boot_spd_caps(h, d_spd)) BUG();

	/* Increment the fork.cnt in source and d_spd struct spd.
	 * FIXME: are different forks comparable system-wide? */
	cos_spd_cntl(COS_SPD_INC_FORK_CNT, source, 1, 0);
	cos_spd_cntl(COS_SPD_INC_FORK_CNT, d_spd, 1, 0);

	/* FIXME: better way to pick threads out. this will get the first
	 * thread, preference to blocked, then search inv stk. The
	 * returned thread is blocked first, to avoid having it run
	 * while quarantining. */
	printl("Getting thread from %d\n", source);
	d_thd = sched_get_thread_in_spd(cos_spd_id(), source, 0);
	/* TODO: instead of blocking the thread, perhaps it can run through
	 * the end of its current invocation? */

	/* inform servers about fork */
	/* should iterate the forked spd's deps and inform each? */
	/* mman */
	if (tot > SERVICE_SIZE) tot = SERVICE_SIZE + 3 * round_up_to_pgd_page(1) - tot;
	else tot = SERVICE_SIZE - tot;
	printl("Telling mman to fork(%d, %d, %d, %x, %d)\n", cos_spd_id(), source, d_spd, prev_map + PAGE_SIZE, tot);
	
	r = mman_fork_spd(cos_spd_id(), source, d_spd, prev_map + PAGE_SIZE, tot);
	if (r) printc("Error (%d) in mman_fork_spd\n", r);

	/* cbuf */
	printl("Telling cbuf to fork(%d, %d, %d)\n", cos_spd_id(), source, d_spd);
	r = cbuf_fork_spd(cos_spd_id(), source, d_spd);
	if (r) printc("Error (%d) in cbuf_fork_spd\n", r);

	quarantine_migrate(cos_spd_id(), source, d_spd, d_thd);
	//if (cos_upcall(d_spd, NULL)) printl("Upcall failed\n");

	printl("Waking up thread %d\n", d_thd);
	if (d_thd) {
		sched_quarantine_wakeup(cos_spd_id(), d_thd);
	}
done:
	printl("Forked %d -> %d\n", source, d_spd);
	return d_spd;
}

