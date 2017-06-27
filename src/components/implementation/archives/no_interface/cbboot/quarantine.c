#include "quarantine_impl.h"

#if defined(DEBUG)
#define printd(...) printc("Quarantine: "__VA_ARGS__)
#else
#define printd(...)
#endif

// #define QUARANTINE_MIGRATE_THREAD

/* spd_map stores the spds of an spd's forks in a linked list. */
struct spd_map_entry {
	spdid_t fork_spd;
	struct spd_map_entry *p, *n;
};
#define SPD_MAP_NELEM (PAGE_SIZE/sizeof(struct spd_map_entry))
static struct spd_map_entry spd_map[SPD_MAP_NELEM];
static struct spd_map_entry spd_map_freelist[SPD_MAP_NELEM];
static int spd_map_freelist_index = 0;

static int
quarantine_add_to_spd_map(int o_spd, int f_spd) {
	/* Where o_spd is origin and f is forked */
	struct spd_map_entry *f;
	assert(spd_map_freelist_index < SPD_MAP_NELEM);		
	f = &spd_map_freelist[spd_map_freelist_index++];
	ADD_END_LIST(&spd_map[o_spd], f, n, p);
	f->fork_spd = f_spd;
	spd_map[f_spd].fork_spd = o_spd; /* store o in fork_spd of list head */
	return f_spd;
}

static struct spd_map_entry*
quarantine_get_spd_map_entry(int o_spd) {
	return &spd_map[o_spd];
}

static int
quarantine_search_spd_map(int f_spd) {
	return spd_map[f_spd].fork_spd;
}	

/* 
 * Called after fork() finished copying the spd from source to target.
 */
int
quarantine_migrate(spdid_t spdid, spdid_t source, spdid_t target, thdid_t thread)
{
	printd("Migrate thread %d in spd %d to %d\n", thread, source, target);
	if (!thread) goto done;

	sched_quarantine_thread(cos_spd_id(), source, target, thread);

done:
	return 0;
}

/* 
 * Increment all of the (non fault handler) caps fork.cnt.
 * GB: unused and might be removed, along with COS_CAP_INC_FORK_CNT 
 */
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

/* 
 * Translates a virtual address in (spdid) component's address space into the
 * quarantine manager's address space. Useful for sharing data between a
 * cbbooted-component and the quarantine manager. 
 */
void* quarantine_translate_addr(spdid_t spdid, vaddr_t addr)
{
	struct cobj_header *h;
	struct cbid_caddr *sect_cbufs;
	int i;

	/* get the memory for spdid component */
	if (unlikely(!(sect_cbufs = cos_vect_lookup(&spd_sect_cbufs, spdid))))
		printd("cos_vect_lookup(%d) in spd_sect_cbufs failed\n", spdid);
	if (unlikely(!(h = cos_vect_lookup(&spd_sect_cbufs_header, spdid))))
		printd("cos_vect_lookup(%d) in spd_sect_cbufs_header failed\n", spdid);
	if (!sect_cbufs || !h) BUG();

	for (i = 0; i < (int)h->nsect; i++) {
		struct cobj_sect *sect = cobj_sect_get(h, i);
		vaddr_t d_addr = sect->vaddr;
		u32_t size = cobj_sect_size(h, i);

		if (addr < d_addr + size) { /* found the right section */
			char* base = (char*)sect_cbufs[i].caddr;
			u32_t offset = addr - d_addr;
			printd("quarantine_translate_addr(%d, %x) got %x\n", spdid, addr, base + offset);
			return base + offset;
		}
	}

	printd("quarantine_translate_addr(%d, %x) failed\n", spdid, addr);
	return NULL;
}

/*
 * Increment send-side fork counters in the capability struct of o_spd.
 * o_spd - origin spd
 * f_spd - fork   spd
 * o_hdr - o_spd's section header
 */
static int
fix_send_side_counters(spdid_t o_spd, spdid_t f_spd, struct cobj_header *o_hdr)
{
	int j;
	struct cobj_cap *cap;

	/* Increment send-side fork count in o_spd's caps, and copy fork count into f_spd's caps. */
	for (j = 0; j < o_hdr->ncap; j++) {
		int cnt;
		cap = cobj_cap_get(o_hdr, j);
		if (cobj_cap_undef(cap)) break;	
		if (cos_cap_cntl(COS_CAP_INC_FORK_CNT, o_spd, cap->cap_off, (1 << 8) | 0)) BUG();
		cnt = cos_cap_cntl(COS_CAP_GET_FORK_CNT, o_spd, cap->cap_off, 0);
		assert(cnt > 0);
		if (cos_cap_cntl(COS_CAP_SET_FORK_CNT, f_spd, cap->cap_off, cnt)) BUG();
		printd("Updated fork count (send-side) for cap %d from spd %d to count %d\n", j, o_spd, cnt);
	}

	return 0;
}
	
/*
 * Currently a horrible hack that routes all calls to O to F instead.
 * Fix this before moving on to a new project or it'll just be left for the next person.
 * On the plus side, only has two parameters, so you know, there's that.
 */
static int
fix_receive_side_counters(spdid_t o_spd, spdid_t f_spd)
{
	int j;
	int i;
	int ncaps;
	int check;

	printd("Routing calls from O (%d) to F (%d)\n", o_spd, f_spd);

	/* Find every spd that has an invocation cap to source and update the receive-side fork count. */
	for (j = 0; cgraph_client(j) != -1; j++) {
		spdid_t spd_client, spd_server;
		
		if (cgraph_server(j) == o_spd) {
			spd_client = cgraph_client(j);
			ncaps = cos_cap_cntl(COS_CAP_GET_SPD_NCAPS, spd_client, 0, 0);
			for (i = 0; i < ncaps; i++) {
				spd_server = cos_cap_cntl(COS_CAP_GET_DEST_SPD, spd_client, i, 0);

				if (spd_server < 0) BUG();
				if (spd_server == o_spd) {
					/* 
					 * TODO: Put this back in. DO NOT move on to another project with this hack still in place.
					 * if (cos_cap_cntl(COS_CAP_INC_FORK_CNT, c, i, (0 << 8) | 1)) BUG();
					 */
					if (cos_cap_cntl(COS_CAP_SET_DEST, spd_client, i - 1, f_spd)) BUG();
					printd("Updated fork count (receive-side) for cap %d from %d->%d to count %d\n", i, spd_client, spd_server, 1);
				}
			}
		}
	}

	return 0;
}

/* quarantine process source if spdid has permission */
spdid_t
quarantine_fork(spdid_t spdid, spdid_t source)
{
	spdid_t d_spd = -1;	/* default failure */

#ifdef QUARANTINE_MIGRATE_THREAD
	thdid_t d_thd;
#endif
	struct cbid_caddr *old_sect_cbufs, *new_sect_cbufs;
	struct cobj_header *src_hdr;
	struct cobj_sect *sect;
	struct cobj_cap *cap;
	vaddr_t init_daddr, cinfo_addr;				/* Important to remember cinfo is in the address space of cbuf_mgr. */
	long tot = 0;
	int j, r;
	unsigned long cinfo_cbid = 0;
	int reset;

	// how to fix this? Can quarantine have an init method
	/* FIXME: initialization hack. */
	static int first = 1;

	int p = sched_curr_set_priority(0);

	if (first) {
		for (j = 0; j < SPD_MAP_NELEM; j++) {
			INIT_LIST(&spd_map[j], n, p);
			spd_map[j].fork_spd = 0;
		}
		memset(spd_map_freelist, 0, PAGE_SIZE);
		first = 0;
	}

	printd("quarantine_fork(%d, %d)\n", spdid, source);

	if (!(old_sect_cbufs = cos_vect_lookup(&spd_sect_cbufs, source)))
		printd("cos_vect_lookup(%d) in spd_sec_cbufs failed\n", source);
	if (!(src_hdr = cos_vect_lookup(&spd_sect_cbufs_header, source)))
		printd("cos_vect_lookup(%d) in spd_sec_cbufs_header failed\n", source);
	if (!old_sect_cbufs || !src_hdr) BUG();
	
	/* helps O out of critical section */
	lock_help_owners(spdid, source);

	/* The following, copied partly from booter.c */
	d_spd = cos_spd_cntl(COS_SPD_CREATE, 0, 0, 0);
	if (d_spd > MAX_NUM_SPDS) goto error;

	printd("Created new spd: %d\n", d_spd);

	sect = cobj_sect_get(src_hdr, 0);
	init_daddr = sect->vaddr;
	if (cos_spd_cntl(COS_SPD_LOCATION, d_spd, sect->vaddr, SERVICE_SIZE)) goto error;
	printd("Set location to %x\n", (unsigned long)sect->vaddr);
	
	if (cos_spd_cntl(COS_SPD_SET_FORK_ORIGIN, d_spd, source, 0)) goto error;
	printd("Set fork origin to %d\n", source);

	// This doesn't happen for at least this experiment, so ignore that
	for (j = 0 ; j < (int)src_hdr->nsect ; j++) {
		tot += cobj_sect_size(src_hdr, j);
	}
	if (tot > SERVICE_SIZE) {
		if (cos_vas_cntl(COS_VAS_SPD_EXPAND, d_spd, sect->vaddr + SERVICE_SIZE, 3 * round_up_to_pgd_page(1))) {
			goto error;
		}
	}

	new_sect_cbufs = cos_vect_lookup(&spd_sect_cbufs, d_spd);
	if (!new_sect_cbufs) {
		new_sect_cbufs = &all_spd_sect_cbufs[all_cbufs_index];
		all_cbufs_index += src_hdr->nsect;
		assert(all_cbufs_index < CBUFS_PER_PAGE * SECT_CBUF_PAGES);
		if (cos_vect_add_id(&spd_sect_cbufs, new_sect_cbufs, d_spd) < 0) goto error;
		if (cos_vect_add_id(&spd_sect_cbufs_header, src_hdr, d_spd) < 0) goto error;
		printd("Added %d to sect_cbufs\n", d_spd);
	}

	vaddr_t prev_map = 0;
	for (j = 0 ; j < (int)src_hdr->nsect ; j++) {
		vaddr_t d_addr;
		struct cbid_caddr cbm;
		cbuf_t cbid;
		int flags;
		int left;

		sect = cobj_sect_get(src_hdr, j);
		d_addr = sect->vaddr;

		left       = cobj_sect_size(src_hdr, j);
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
			flags |= MAPPING_NO_VALLOC;
			cbm = old_sect_cbufs[j];

			if (sect->flags & COBJ_SECT_CINFO) {
				/* fixup cinfo page */
				struct cos_component_information *ci = cbm.caddr;
				cinfo_cbid = cbm.cbid;
				__boot_deps_save_hp(d_spd, ci->cos_heap_ptr);
			}
			prev_map += left - PAGE_SIZE;
			d_addr += left;
		}
	}

	/* FIXME: better way to pick threads out. this will get the first
	 * thread, preference to blocked, then search inv stk. The
	 * returned thread is blocked first, to avoid having it run
	 * while quarantining. */
	/* TODO: migrating threads from the source to its fork should be
	 * controlled by a policy that includes what threads (blocked,
	 * running, etc), and how many to migrate */
#ifdef QUARANTINE_MIGRATE_THREAD
	thdid_t c_thd = cos_get_thd_id();  // the id of the current thread
	d_thd = sched_get_thread_in_spd(cos_spd_id(), source, 0);
#endif

	/* 
	 * Inform servers about fork. Have to let servers update
	 * spdid-based metadata before either the source (o) or d_spd (fork)
	 * make invocations to the servers. This is also done lazily through
	 * the upcall mechanism in the fault handling path. 
	 */
	if (tot > SERVICE_SIZE) tot = SERVICE_SIZE + 3 * round_up_to_pgd_page(1) - tot;
	else tot = SERVICE_SIZE - tot;
	
	cinfo_addr = cbuf_fork_spd(cos_spd_id(), source, d_spd, cinfo_cbid);
	
	/* fixup cinfo page */
	struct cos_component_information *ci = (struct cos_component_information*) cinfo_addr;
	ci->cos_this_spd_id = d_spd;
	__boot_spd_set_symbs(src_hdr, d_spd, ci);
	__boot_deps_save_hp(d_spd, ci->cos_heap_ptr);
	
	/* 
	 * FIXME: set fault handlers and re-write caps 
	 * Also a note that activate should come after cbuf_fork, since it expects memory to be copied over.
	 */
	if (cos_spd_cntl(COS_SPD_ACTIVATE, d_spd, src_hdr->ncap, 0)) BUG();
	if (__boot_spd_caps(src_hdr, d_spd)) BUG();
	
	/* Fix send-side fork counters */
	//if (fix_send_side_counters(source, d_spd, src_hdr)) BUG();
	//if (fix_receive_side_counters(source, d_spd)) BUG();

	quarantine_add_to_spd_map(source, d_spd);

#ifdef QUARANTINE_MIGRATE_THREAD
	//quarantine_migrate(cos_spd_id(), source, d_spd, d_thd);
	//if (d_thd) {
	//	sched_quarantine_wakeup(cos_spd_id(), d_thd);
	//}
#endif

	/* TODO: should creation of boot threads be controlled by policy? */
	printd("Creating boot threads in fork: %d\n", d_spd);
	if (__boot_spd_thd(d_spd)) BUG();
	
	reset = sched_curr_set_priority(p);
	assert(0 == reset);
	printd("Forked %d -> %d\n", source, d_spd);
	return d_spd;

error:
	printc("Found an error\n");
	d_spd = 0;
	return d_spd;
}

/*
 * Handles routing of calls
 */
int
fault_quarantine_handler(spdid_t spdid, long cspd_dspd, int cap_ccnt_dcnt, void *ip)
{
	unsigned long r_ip;
	int tid;
	u16_t capid;
	s8_t c_fix, d_fix;
	int c_spd, d_spd;
	int f_spd;
	int inc_val;
	int fork_count;

	// unpack
	capid = cap_ccnt_dcnt >> 16;
	d_fix = (cap_ccnt_dcnt >> 8) & 0xff; 	/* fix the d (server) if snd != 0 */
	c_fix = cap_ccnt_dcnt & 0xff; 		/* fix the c (client) if rcv != 0 */
	c_spd = cspd_dspd >> 16;
	d_spd  = cspd_dspd & 0xffff;

	printd("quarantine.c fault_quarantine_handler %d (%d) -> %d (%d)\n", c_spd, c_fix, d_spd, d_fix);

	if (d_fix) {
		/* 
		 * Either c_spd is a fork, or c_spd has been forked. Either way,
		 * the server (d_spd) needs to have its metadata related to c_spd
		 * fixed. The server possibly can determine the case (fork, forkee). 
		 */

		/* if we know for sure that c_spd is the fork, then we can
		 * search for the o_spd. If c_spd is the forkee (o_spd)
		 * then the following will work. But usually we expect that
		 * c_spd is the fork, since that request is most likely next
		 * to happen. FIXME: Except that d_spd may not have a dep to
		 * the Quarantine Manager, so it may not be able to inquire. 
		 */
		/* 
		 *Assuming c_spd is the forked component, so find the orig 
		 */
		struct spd_map_entry *c_map_entry = quarantine_get_spd_map_entry(c_spd);
		if (EMPTY_LIST(c_map_entry, n, p)) {
			/* no forks for c_spd, it must be the fork, and its
			 * origin is stored in its fork_spd field. */
			f_spd = c_spd;
			c_spd = c_map_entry->fork_spd;
			if (unlikely(c_spd == 0)) {
				printd("No original found for forked spd %d\n", f_spd);
			}
		} else {
			/* forks for c_spd found. FIXME: which one to use? */
			f_spd = LAST_LIST(c_map_entry, n, p)->fork_spd;
		}
		
		printd("Fixing server %d's metadata for spd %d after fork to %d\n", d_spd, f_spd, c_spd);

		int fork_count = cos_cap_cntl(COS_CAP_GET_FORK_CNT, c_spd, capid, 0);
		upcall_invoke(cos_spd_id(), COS_UPCALL_QUARANTINE, d_spd, (f_spd<<16)|c_spd);
	}
	if (c_fix) {
		/* d_spd has been forked, and c_spd needs to have its inv caps fixed.
		 * Two possible ways to fix c_spd are to (1) find the usr_cap_tbl and
		 * add a capability for the fork directly, or (2) add a syscall to
		 * do the same. The following uses a syscall, since after adding the
		 * capability to the usr_cap_tbl a syscall is needed anyway to fix
		 * the struct spd caps[], ncaps. So just do it once.
		 */
		struct spd_map_entry *d = quarantine_get_spd_map_entry(d_spd);
		if (unlikely(EMPTY_LIST(d, n, p))) {
			printd("No forks found for %d\n", d_spd);
			goto dont_fix_c;
		}

		/* FIXME: which fork should be used here? This pulls the most
		 * recent fork. */
		f_spd = LAST_LIST(d, n, p)->fork_spd;
		printd("Fixing routing table after fork from %d -> %d\n",
				d_spd, f_spd);

		// TODO: add / change ucap, routing table
		// should this do anything else???? Feel like it probably should
		// f_spd is then never used. So that's bad.
	}
dont_fix_c:
	/* Adjust the fork count by the observed amount. We could just set
	 * this to zero, but what if a fork has happened since the fault
	 * handler was invoked? Probably we want to just decrement, and let
	 * the fault happen again in this (unlikely) case. */
	/* FIXME: What, if anything, can we do about other caps between
	 * c_spd and d_spd? */

	// This is a terrible hack to cover up for another hack. Fix the other hack instead. Clearly the d_fix/c_fix are wrong. Find out why and fix that.
	fork_count = cos_cap_cntl(COS_CAP_GET_FORK_CNT, c_spd, capid, 0);
	//printc("Fork count at %d\n", fork_count);
	int cur_d, cur_c;
	cur_d = (fork_count >> 8) & 0xff;
	cur_c = fork_count & 0xff;
	if (cur_d - d_fix < 0) d_fix = -cur_d;
	if (cur_c - c_fix < 0) c_fix = -cur_c;
	
	inc_val = (((u8_t)-d_fix)<<8U) | ((u8_t)(-c_fix));
	printd("Incrementing fork count by %d (which works out to d %d and c %d) in spd %d for cap %d\n", inc_val, -d_fix, -c_fix, c_spd, capid);
	cos_cap_cntl(COS_CAP_INC_FORK_CNT, c_spd, capid, inc_val);
	
	fork_count = cos_cap_cntl(COS_CAP_GET_FORK_CNT, c_spd, capid, 0);
	//printc("Fork count at %d\n", fork_count);

	tid = cos_get_thd_id();
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

