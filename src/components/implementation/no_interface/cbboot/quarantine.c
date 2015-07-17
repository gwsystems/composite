
#include "quarantine_impl.h"

#define printl(...) printc("Quarantine: "__VA_ARGS__)

/* quarantine_spd_map stores the spds of an spd's forks.
 * Currently this is implemented as a simple linear array indexed by the
 * original spd's id.
 *
 * FIXME: need a smarter data structure here to allow forking the same
 * component multiple times */
#define SPD_MAP_NELEM (PAGE_SIZE/sizeof(spdid_t))
spdid_t quarantine_spd_map[SPD_MAP_NELEM];

static int
quarantine_add_to_spd_map(int orig_spd, int fork_spd) {
	/* FIXME: need something other than a flat array... */
	assert(quarantine_spd_map[orig_spd] == 0);
	return quarantine_spd_map[orig_spd] = fork_spd;
}

static int
quarantine_get_spd_map(int orig_spd) {
	return quarantine_spd_map[orig_spd];
}

static int
quarantine_search_spd_map(int fork_spd) {
	int i = 0;
	/* Terrible linear search */
	while (quarantine_spd_map[i] != fork_spd) {
		if (++i == SPD_MAP_NELEM) return -1;
	}
	return i;
}


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

	/* FIXME: initialization hack. */
	static int first = 1;
	if (first) { memset(quarantine_spd_map, 0, PAGE_SIZE); first = 0; }

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

	/* inform servers about fork. Have to let servers update
	 * spdid-based metadata before either the source (orig) or d_spd (fork)
	 * make invocations to the servers. This is also done lazily through
	 * the upcall mechanism in the fault handling path. */
	/* should iterate the forked spd's deps and inform each? */
	/* mman: have to do this now so the memory maps are available. */
	if (tot > SERVICE_SIZE) tot = SERVICE_SIZE + 3 * round_up_to_pgd_page(1) - tot;
	else tot = SERVICE_SIZE - tot;
	printl("Telling mman to fork(%d, %d, %d, %x, %d)\n", cos_spd_id(), source, d_spd, prev_map + PAGE_SIZE, tot);
	
	r = mman_fork_spd(cos_spd_id(), source, d_spd, prev_map + PAGE_SIZE, tot);
	if (r) printc("Error (%d) in mman_fork_spd\n", r);

#if 0
	/* cbuf */
	printl("Telling cbuf to fork(%d, %d, %d)\n", cos_spd_id(), source, d_spd);
	r = cbuf_fork_spd(cos_spd_id(), source, d_spd);
	if (r) printc("Error (%d) in cbuf_fork_spd\n", r);

	/* TODO: valloc */
#endif

	quarantine_migrate(cos_spd_id(), source, d_spd, d_thd);
	//if (cos_upcall(d_spd, NULL)) printl("Upcall failed\n");

	printl("Waking up thread %d\n", d_thd);
	if (d_thd) {
		sched_quarantine_wakeup(cos_spd_id(), d_thd);
	}
done:
	printl("Forked %d -> %d\n", source, d_spd);
	quarantine_add_to_spd_map(source, d_spd);
	return d_spd;
}


int
fault_quarantine_handler(spdid_t spdid, long cspd_dspd, int ccnt_dcnt, void *ip)
{
	unsigned long r_ip;
	int tid = cos_get_thd_id();
	int c_spd, d_spd, c_cnt, d_cnt;
	int f_spd;
	c_cnt = ccnt_dcnt>>16;
	d_cnt = ccnt_dcnt&0xffff;
	c_spd = cspd_dspd>>16;
	d_spd = cspd_dspd&0xffff;

	printl("fault_quarantine_handler %d (%d) -> %d (%d)\n", c_spd, c_cnt, d_spd, d_cnt);

	if (c_cnt) {
	/* Either c_spd is a fork, or c_spd has been forked. Either way,
	 * the server (d_spd) needs to have its metadata related to c_spd
	 * fixed. The server can determine the case (fork, forkee). */
		/* if we know for sure that c_spd is the fork, then we can
		 * (linear) search for the o_spd. If c_spd is the forkee (o_spd)
		 * then the following will work. But usually we expect that
		 * c_spd is the fork, since that request is most likely next
		 * to happen. FIXME: Except that d_spd may not have a dep to
		 * the Quarantine Manager, so it may not be able to inquire. */
		/* Assuming c_spd is the forked component, so find the orig */
		f_spd = quarantine_search_spd_map(c_spd);
		
		printl("Fixing server %d's metadata for spd %d after fork to %d\n", d_spd, f_spd, c_spd);
	
		upcall_invoke(cos_spd_id(), COS_UPCALL_QUARANTINE, d_spd, (f_spd<<16)|c_spd);

		cos_spd_cntl(COS_SPD_INC_FORK_CNT, c_spd, -c_cnt, 0);
	}
	if (d_cnt) {
	/* d_spd has been forked, and c_spd needs to have its inv caps fixed.
	 * Two possible ways to fix c_spd are to (1) find the usr_cap_tbl and
	 * add a capability for the fork directly, or (2) add a syscall to
	 * do the same. The following uses a syscall, since after adding the
	 * capability to the usr_cap_tbl a syscall is needed anyway to fix
	 * the struct spd caps[], ncaps. So just do it once.
	 */
		f_spd = quarantine_get_spd_map(d_spd);
		printl("Fixing routing table after fork from %d -> %d\n",
				d_spd, f_spd);

		// TODO: add / change ucap, routing table
		
		cos_spd_cntl(COS_SPD_INC_FORK_CNT, d_spd, -d_cnt, 0);
	}

	/* remove from the invocation stack the faulting component! */
	assert(!cos_thd_cntl(COS_THD_INV_FRAME_REM, tid, 1, 0));

	/* Manipulate the return address of the component that called
	 * the faulting component... */
	assert(r_ip = cos_thd_cntl(COS_THD_INVFRM_IP, tid, 1, 0));
	/* ...and set it to its value -8, which is the fault handler
	 * of the stub. */
	assert(!cos_thd_cntl(COS_THD_INVFRM_SET_IP, tid, 1, r_ip-8));

	return COS_FLT_QUARANTINE;
}

