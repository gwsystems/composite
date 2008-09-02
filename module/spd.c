/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

//#include <spd.h>
#include "include/spd.h"
#include "include/debug.h"
#include "include/page_pool.h"
#include <linux/kernel.h>

/* 
 * This is the layout in virtual memory of the spds.  Spd's virtual
 * ranges are allocated (currently) on the granularity of a pgd, thus
 * an array of pointers, one for every pgd captures all address->spd
 * mappings.
 */
struct spd *virtual_spd_layout[PGD_PER_PTBL];

int virtual_namespace_alloc(struct spd *spd, unsigned long addr, unsigned int size)
{
	int i;
	unsigned long adj_addr = addr>>HPAGE_SHIFT;
	/* FIXME: this should be rounding up not down */
	unsigned int adj_to = adj_addr + (size>>HPAGE_SHIFT);

	for (i = adj_addr ; i < adj_to ; i++) {
		if (virtual_spd_layout[i]) return 0;
	}

	//printk("cos: adding spd %d from %x to %x\n", spd_get_index(spd), addr, addr+size);
	for (i = adj_addr ; i < adj_to ; i++) {
		virtual_spd_layout[i] = spd;
	}
	
	return 1;
}

struct spd *virtual_namespace_query(unsigned long addr)
{
	unsigned long adj = addr>>HPAGE_SHIFT;

	return virtual_spd_layout[adj];
}


struct invocation_cap invocation_capabilities[MAX_STATIC_CAP];

int cap_is_free(int cap_num)
{
	if (cap_num < MAX_STATIC_CAP &&
	    invocation_capabilities[cap_num].owner == CAP_FREE) {
		return 1;
	}
	
	return 0;
}

static inline void cap_set_free(int cap_num)
{
	invocation_capabilities[cap_num].owner = CAP_FREE;
}

static void spd_init_capabilities(struct invocation_cap *caps)
{
	int i;

	for (i = 0 ; i < MAX_STATIC_CAP ; i++) {
		caps[i].owner = CAP_FREE;
		caps[i].destination = NULL;
	}

	return;
}

/*
 * Returns the index of the first free entry in the
 * invocation_capabilities array, where a range of free entries size
 * "range" is based..
 *
 * FIXME: this should be really implemented as a free-list of blocks
 * of different free ranges, so that an iteration through the freelist
 * would find us an empty region, rather than an iteration through all
 * entries.
 */
static short int spd_alloc_capability_range(int range)
{
	short int i, range_free = 0;

	for (i = 0 ; i < MAX_STATIC_CAP ; i++) {
		//struct invocation_cap *cap = &invocation_capabilities[i];

		if (/*cap->owner == NULL*/cap_is_free(i)) {
			range_free++;

			/* 
			 * If we have found an acceptable range, then
			 * go through and mark ownership of all of the
			 * entries.
			 */
			if (range_free >= range) {
				int start, j;
				start = i - range_free + 1;

				for (j = start ; j <= i; j++) {
					struct invocation_cap *init = &invocation_capabilities[j];
					init->owner = CAP_ALLOCATED_UNUSED;
				}

				return i-range_free+1;
			}
		} else {
			range_free = 0;
		}
	}

	return -1;
}

/*
 * FIXME: use copy_to_user
 */
static inline void cap_set_usr_cap(struct usr_inv_cap *uptr, vaddr_t inv_fn, 
				   unsigned int inv_cnt, unsigned int cap_no)
{
	uptr->invocation_fn = inv_fn;
	uptr->service_entry_inst = inv_fn;
	uptr->invocation_count = inv_cnt;
	uptr->cap_no = cap_no;
	//printk("cos: writing to user-level capno %x, %x (%x)\n", cap_no, cap_no>>16, inv_fn);

	return;
}

static inline struct usr_inv_cap *cap_get_usr_cap(int cap_num)
{
	struct spd *owner;

	if (cap_is_free(cap_num)) {
		return NULL;
	}

	owner = invocation_capabilities[cap_num].owner;
	return &owner->user_cap_tbl[cap_num];
}

static inline int cap_reset_cap_inv_cnt(int cap_num, int *inv_cnt)
{
	struct usr_inv_cap *ucap;
	struct invocation_cap *cap;

	ucap = cap_get_usr_cap(cap_num);
	if (!ucap) return -1;
	cap = &invocation_capabilities[cap_num];
	*inv_cnt = ucap->invocation_count + cap->invocation_cnt;
	ucap->invocation_count = cap->invocation_cnt = 0;

	return 0;
}

/* 
 * FIXME: access control
 */
isolation_level_t cap_change_isolation(int cap_num, isolation_level_t il, int flags)
{
	isolation_level_t prev;
	struct invocation_cap *cap;
	struct spd *owner;
	struct usr_inv_cap *ucap;

	if (cap_num >= MAX_STATIC_CAP) {
		printk("Attempting to change isolation level of invalid cap %d.\n", cap_num);
		return IL_INV;
	}

	cap = &invocation_capabilities[cap_num];
	prev = cap->il;
	//if (prev == il) return prev;

	cap->il = il;

	owner = cap->owner;

/*  	printk("cos: change to il %s for cap %d with owner %d.\n",  */
/* 	       ((il == IL_SDT) ? "SDT" : ((il == IL_AST) ? "AST" : ((il == IL_ST) ? "ST" : "INV"))),  */
/* 	       cap_num, spd_get_index(owner)); */

	/* Use the kernel vaddr space table if possible, but it might
	 * not be available on initialization */
	ucap = (NULL != owner->user_cap_tbl) ? 
		&owner->user_cap_tbl[cap_num - owner->cap_base]:
		&owner->user_vaddr_cap_tbl[cap_num - owner->cap_base];

	if (flags & CAP_SAVE_REGS) {
		/* set highest order bit to designate saving of
		 * registers. */
		cap_num = (cap_num<<20)|0x80000000; 
	} else {
		cap_num <<= 20;
	}

	switch (il) {
	case IL_SDT:
//		printk("cos:\tSDT w/ entry %x.\n", (unsigned int)cap->usr_stub_info.SD_serv_stub);
		cap->dest_entry_instruction = cap->usr_stub_info.SD_serv_stub;
		cap_set_usr_cap(ucap, cap->usr_stub_info.SD_cli_stub, 0, cap_num);
		break;
	case IL_AST:
//		printk("cos:\tAST w/ entry %x.\n", (unsigned int)cap->usr_stub_info.AT_serv_stub);
		cap->dest_entry_instruction = cap->usr_stub_info.AT_serv_stub;
		cap_set_usr_cap(ucap, cap->usr_stub_info.AT_cli_stub, 0, cap_num);
		break;
	case IL_ST:
//		printk("cos:\tST w/ entry %x.\n", (unsigned int)cap->usr_stub_info.ST_serv_entry);
		cap->dest_entry_instruction = cap->usr_stub_info.ST_serv_entry;
		cap_set_usr_cap(ucap, cap->usr_stub_info.ST_serv_entry, 0, cap_num);
		break;
	default:
		return IL_INV;
	}

	return prev;
}

struct spd spds[MAX_NUM_SPDS];

/*
 * This should be a per-cpu data structure and should be protected
 * with int_(enable|disable).
 */
static struct spd *spd_freelist_head = NULL;

static void spd_init_all(struct spd *spds)
{
	int i;
	
	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		spds[i].spd_info.flags = SPD_FREE;
		spds[i].composite_spd = &spds[i].spd_info;
		spds[i].freelist_next = (i == (MAX_NUM_SPDS-1)) ? NULL : &spds[i+1];
	}

	spd_freelist_head = spds;

	for (i = 0 ; i < PGD_PER_PTBL ; i++) {
		virtual_spd_layout[i] = NULL;
	}

	return;
}
	
void spd_init(void)
{
	spd_init_all(spds);
	spd_init_capabilities(invocation_capabilities);
	spd_init_mpd_descriptors();
}

int spd_is_free(int idx)
{
	return (spds[idx].spd_info.flags & SPD_FREE) ? 1 : 0;
}

void spd_mpd_release(struct composite_spd *cspd);
static void spd_mpd_terminate(struct composite_spd *cspd);
void spd_free(struct spd *spd)
{
	assert(spd);

	//spd_free_mm(spd);
	spd->spd_info.flags = SPD_FREE;
	spd->freelist_next = spd_freelist_head;
	spd_freelist_head = spd;

	if (spd->composite_spd != &spd->spd_info) {
		struct composite_spd *cspd = (struct composite_spd *)spd->composite_spd;
		assert(cspd);// && !spd_mpd_is_depricated(cspd));

		//spd_mpd_release((struct composite_spd*)spd->composite_spd);
		spd_mpd_terminate(cspd);
	}

	return;
}

extern int spd_free_mm(struct spd *spd);
void spd_mpd_free_all(void);
void spd_free_all(void)
{
	int i;
	
	clear_pg_pool();

	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		if (!(spds[i].spd_info.flags & SPD_FREE)) {
			spd_free_mm(&spds[i]);
			spd_free(&spds[i]);
		}
	}

	spd_mpd_free_all();
	spd_init();
}

struct spd *spd_alloc(unsigned short int num_caps, struct usr_inv_cap *user_cap_tbl,
		      vaddr_t upcall_entry)
{
	struct spd *spd;
	int ret;

	spd = spd_freelist_head;

	if (spd == NULL) {
		printd("cos: no freelist!\n");
		return NULL;
	}

	/* remove from freelist */
	spd_freelist_head = spd_freelist_head->freelist_next;
	spd->spd_info.flags = 0; /* &= ~SPD_FREE */

	/* +1 for the (dummy) 0th return cap */
	ret = spd_alloc_capability_range(num_caps+1);

	if (ret == -1) {
		printd("cos: could not allocate a capability range.\n");
		goto free_spd;
	}
	
	spd->cap_base = (unsigned short int)ret;
	spd->cap_range = num_caps+1;

	spd->user_cap_tbl = NULL; //user_cap_tbl;
	spd->user_vaddr_cap_tbl = user_cap_tbl;

	spd->upcall_entry = upcall_entry;
	spd->sched_shared_page = NULL;
	
	/* This will cause the spd to never be deallocated via garbage collection.
	   Update: not really doing ref counting on spds. */
	//cos_ref_take(&spd->spd_info.ref_cnt); 
	//cos_ref_take(&spd->spd_info.ref_cnt); 

	/* return capability; ignore return value as we know it will be 0 */
	spd_add_static_cap(spd, 0, spd, 0);

	spd->composite_spd = &spd->spd_info;

	spd->sched_depth = -1;
	spd->parent_sched = NULL;

	return spd;

 free_spd:
	spd_free(spd);
	return NULL;
}

/* int spd_get_index(struct spd *spd) */
/* { */
/* 	return ((unsigned long)spd-(unsigned long)spds)/sizeof(struct spd); */
/* } */

/*
 * Does an address range fit on a single page?
 */
static int user_struct_fits_on_page(unsigned long addr, unsigned int size)
{
	unsigned long start, end;

	start = addr & PAGE_MASK;
	end = (addr+size) & PAGE_MASK;

	return (start == end);
}

int pages_identical(unsigned long *addr1, unsigned long *addr2)
{
	unsigned long saved_val;

	saved_val = *addr1;
	if (*addr2 != saved_val) return 0;
	*addr1 = 0xdeadbeef;
	if (*addr2 != 0xdeadbeef) return 0;
	*addr1 = saved_val;

	return 1;
}

/*
 * When calling this method, the page-table must be configured to
 * contain the appropriate ptes for the user-capability tables, so
 * that we can get the kernel virtual address of them.  This implies
 * that the user_vaddr_cap_tbl field in the spd must also be
 * initialized.
 */
extern vaddr_t pgtbl_vaddr_to_kaddr(phys_addr_t pgtbl, unsigned long addr);
int spd_set_location(struct spd *spd, unsigned long lowest_addr, 
		     unsigned long size, phys_addr_t pg_tbl)
{
	vaddr_t kaddr;

	assert(spd);
	assert(spd->user_vaddr_cap_tbl);
	assert(NULL == spd->user_cap_tbl);
	assert(user_struct_fits_on_page((unsigned long)spd->user_vaddr_cap_tbl, 
					sizeof(struct usr_inv_cap) * spd->cap_range));

	spd->spd_info.pg_tbl = pg_tbl;
	spd->location.lowest_addr = lowest_addr;
	spd->location.size = size;

	/* 
	 * We need to reference the kernel virtual address, not user
	 * virtual as we might need to alter this while not in the
	 * spd's page tables (i.e. when merging protection domains).
	 */
	kaddr = pgtbl_vaddr_to_kaddr(pg_tbl, (unsigned long)spd->user_vaddr_cap_tbl);
	if (0 == kaddr) {
		printk("cos: could not translate the user-cap address, %x, into a kernel vaddr for spd %d.\n",
		       (unsigned int)spd->user_vaddr_cap_tbl, spd_get_index(spd));
		return -1;
	}

	spd->user_cap_tbl = (struct usr_inv_cap *)kaddr;

	assert(pages_identical((unsigned long*)spd->user_vaddr_cap_tbl,
			       (unsigned long*)spd->user_cap_tbl));

	return 0;
}

struct spd *spd_get_by_index(int idx)
{
	if (idx >= MAX_NUM_SPDS) {
		return NULL;
	}

	return &spds[idx];
}

/* 
 * Static Capability Manipulation Functions
 */
unsigned int spd_add_static_cap(struct spd *owner_spd, vaddr_t ST_serv_entry, 
				struct spd *trusted_spd, isolation_level_t isolation_level)
{
	return spd_add_static_cap_extended(owner_spd, trusted_spd, 0, ST_serv_entry, 0, 0, 0, 0, isolation_level, 0);
}

/*
 * Return the capability number allocated, or 0 on error (too many
 * static capabilities allocated).
 */
unsigned int spd_add_static_cap_extended(struct spd *owner_spd, struct spd *trusted_spd, 
					 int cap_offset, vaddr_t ST_serv_entry, 
					 vaddr_t AT_cli_stub, vaddr_t AT_serv_stub, 
					 vaddr_t SD_cli_stub, vaddr_t SD_serv_stub,
					 isolation_level_t isolation_level, int flags)
{
	struct invocation_cap *new_cap;
	int cap_num;
	struct usr_cap_stubs *stubs;

	if (!owner_spd || !trusted_spd || owner_spd->user_vaddr_cap_tbl == NULL) {
		printd("cos: Invalid cap request args (%p, %p, %x).\n",
		       owner_spd, trusted_spd, (unsigned int)owner_spd->user_vaddr_cap_tbl);
		return 0;
	}

	/*
	 * static capabilities cannot be added past the hard limit set
	 * at spd allocation time.
	 */
	if (cap_offset >= owner_spd->cap_range ||
	    isolation_level > MAX_ISOLATION_LVL_VAL) {
		printd("cos: Capability out of range (valid [%d,%d), cap # %d, il %x).\n",
		       owner_spd->cap_base, owner_spd->cap_range+owner_spd->cap_base,
		       owner_spd->cap_base + cap_offset, isolation_level);
		return 0;
	}

	cap_num = owner_spd->cap_base + cap_offset;
	new_cap = &invocation_capabilities[cap_num];

	/* If the capability is already in use, error out */
	if (new_cap->owner != CAP_ALLOCATED_UNUSED) {
		printd("cos: capability %d already in use by spd %p.\n",
		       owner_spd->cap_base + cap_offset, new_cap->owner);
		return 0;
	}
	
	stubs = &new_cap->usr_stub_info;

	/* initialize the new capability's information */
	new_cap->owner = owner_spd;
	new_cap->destination = trusted_spd;
	new_cap->invocation_cnt = 0;
	new_cap->il = isolation_level;

	new_cap->dest_entry_instruction = stubs->ST_serv_entry = ST_serv_entry;
	stubs->AT_cli_stub = AT_cli_stub;
	stubs->AT_serv_stub = AT_serv_stub;
	stubs->SD_cli_stub = SD_cli_stub;
	stubs->SD_serv_stub = SD_serv_stub;

	/* and user-level representation (touching user-level pages here) */
//	cap_set_usr_cap(cap_get_usr_cap(usr_cap_num), ST_serv_entry, 0, cap_num);
	if (cap_change_isolation(cap_num, isolation_level, flags) == IL_INV) {
		printk("Unrecognized isolation level for cap # %d.\n", cap_num);
		return 0;
	}

	return cap_num;
}

extern void *va_to_pa(void *va);
extern void *pa_to_va(void *pa);

extern void zero_pgtbl_range(phys_addr_t pt, unsigned long lower_addr, unsigned long size);
extern void copy_pgtbl_range(phys_addr_t pt_to, phys_addr_t pt_from,
			     unsigned long lower_addr, unsigned long size);
extern int pgtbl_entry_absent(phys_addr_t pt, unsigned long addr);

struct composite_spd mpd_descriptors[MAX_MPD_DESC];
struct composite_spd *mpd_freelist;

void spd_init_mpd_descriptors(void)
{
	int i;
//	struct page_list *page;

	mpd_freelist = mpd_descriptors;
	for (i = 0 ; i < MAX_MPD_DESC ; i++) {
		struct composite_spd *cspd = &mpd_descriptors[i];

		cspd->spd_info.flags = SPD_COMPOSITE | SPD_FREE;
		cos_ref_set(&cspd->spd_info.ref_cnt, 0);
		cspd->freelist_next = &mpd_descriptors[i+1];
		cspd->members = NULL;
	}
	mpd_descriptors[MAX_MPD_DESC-1].freelist_next = NULL;

	/* put one page into the page pool */
/*
	page = cos_get_pg_pool();
	assert(NULL != page);
	cos_put_pg_pool(page);
*/
	return;
}

struct composite_spd *spd_mpd_by_idx(short int idx)
{
	if (idx >= MAX_MPD_DESC) return NULL;

	return &mpd_descriptors[idx];
}

short int spd_mpd_index(struct composite_spd *cspd)
{
	short int idx = cspd - mpd_descriptors;

	if (idx >= MAX_MPD_DESC || idx < 0) return -1;

	return idx;
}

extern vaddr_t kern_pgtbl_mapping;
extern void copy_pgtbl_range_nocheck(phys_addr_t pt_to, phys_addr_t pt_from,
				     unsigned long lower_addr, unsigned long size);

short int spd_alloc_mpd_desc(void)
{
	struct composite_spd *new;
	struct page_list *page;

	new = mpd_freelist;
	if (NULL == new) {
		return -1;
	}

	assert((new->spd_info.flags & (SPD_FREE | SPD_COMPOSITE)) == (SPD_FREE | SPD_COMPOSITE));
	assert(0 == cos_ref_val(&new->spd_info.ref_cnt));
	assert(NULL == new->members);

	cos_meas_event(COS_MPD_ALLOC);

	mpd_freelist = new->freelist_next;
	new->freelist_next = NULL;
	new->members = NULL;
	new->spd_info.flags &= ~SPD_FREE;

	page = cos_get_pg_pool();
	if (NULL == page) {
		new->freelist_next = mpd_freelist;
		mpd_freelist = new;
		return -1;
	}

	/* 
	 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
	 *
	 * This should NOT be done.  We should clear out all the spd
	 * entries individually when the page table is released,
	 * instead of going through the _entire_ page and clearing it
	 * with interrupts disabled.  But till we do this the right
	 * way, this ensures correctness.
	 *
	 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
	 */
	copy_pgtbl_range_nocheck((phys_addr_t)va_to_pa(page), 
				 (phys_addr_t)va_to_pa((void*)kern_pgtbl_mapping),
				 0, 0xFFFFFFFF);
	assert(!pgtbl_entry_absent((phys_addr_t)va_to_pa(page), COS_INFO_REGION_ADDR));

	cos_ref_take(&new->spd_info.ref_cnt);
	new->spd_info.pg_tbl = (phys_addr_t)va_to_pa(page);
	
	return spd_mpd_index(new);
}

static inline int spd_mpd_is_subordinate(struct composite_spd *mpd)
{ 
	return (mpd->spd_info.flags & SPD_SUBORDINATE);
}

/*
 * Release use of the composite spd.  This may have the side-effect of
 * deallocating the composite_spd if its refcnt falls to 0.
 */
void spd_mpd_release(struct composite_spd *cspd)
{
	cos_ref_release(&cspd->spd_info.ref_cnt);
	assert(0 <= cos_ref_val(&cspd->spd_info.ref_cnt));

	if (0 == cos_ref_val(&cspd->spd_info.ref_cnt)) {
		assert(cspd->members == NULL);
		assert(spd_mpd_is_depricated(cspd));

		cos_meas_event(COS_MPD_FREE);
		
		cspd->freelist_next = mpd_freelist;
		mpd_freelist = cspd;
		//cspd->members = NULL;
		if (spd_mpd_is_subordinate(cspd)) {
			/*printk("cos: releasing cspd %d subordinate to %d.\n",
			  spd_mpd_index(cspd), spd_mpd_index(cspd->master_spd));*/
			/* this recursive call should only happen once
			 * at most because of the flattening of the
			 * subordinate hierarchy, see
			 * spd_mpd_make_subordinate*/
			spd_mpd_release(cspd->master_spd);
		} else {
			//printk("cos: releasing cspd %d.\n", spd_mpd_index(cspd));
			cos_put_pg_pool(pa_to_va((void*)cspd->spd_info.pg_tbl));
			cspd->spd_info.pg_tbl = 0;
		}
		cspd->spd_info.flags = SPD_FREE | SPD_COMPOSITE;

		//printk("cos: cspd %d being released.\n", spd_mpd_index(cspd));
	} 

	return;
}

static void spd_mpd_subordinate(struct composite_spd *mpd, struct composite_spd *master)
{
	struct composite_spd *m;

	/*
	 * We want a flattened hierarchy of subordinates.  If we have
	 * a tree of subordinates, then the time it takes to walk that
	 * tree when a leaf is freed (causing a recursive free of the
	 * tree node composite spds) is unbounded, and thus not
	 * predictable.  Therefore, if our master is subordinate to
	 * another, then our master should be the master's master.
	 */
	m = (spd_mpd_is_subordinate(master)) ?
		master->master_spd:
		master;

	mpd->spd_info.flags |= SPD_SUBORDINATE;
	mpd->master_spd = m;
	mpd->spd_info.pg_tbl = m->spd_info.pg_tbl;
	cos_ref_take(&m->spd_info.ref_cnt);

	return;
}

/*
 * If one composite spd contains all entries of another, and is more
 * current (the other is depricated), then we can just use the
 * up-to-date superset page tables for both composite spds.  This
 * allows us to deallocate early the page-tables for the old cspd.  We
 * must still keep the old one around if it has any references to it.
 *
 * Make the slave composite spd subordinate to the master in that it
 * will now use the master's page tables, discarding its own.  The
 * master will not be released until the slave has been.  This
 * function is used when all of the slave's spds have been moved over
 * into the master.  Both composite spd's are now essentially copies,
 * so might as well get rid of one of the page-tables.  We have to
 * keep the slave around as it might still be pointed to in the
 * invocation stacks of some threads.  If not, it will be deleted
 * properly.
 */
void spd_mpd_make_subordinate(struct composite_spd *master_cspd, 
			      struct composite_spd *slave_cspd)
{
	assert(!spd_mpd_is_subordinate(slave_cspd));
	assert(0 == spd_composite_num_members(slave_cspd));
	assert(!spd_mpd_is_depricated(master_cspd));

	//printk("cos: depricating cspd %d.\n", spd_mpd_index(slave_cspd));	
	slave_cspd->spd_info.flags |= SPD_DEPRICATED;

	if (1 < cos_ref_val(&slave_cspd->spd_info.ref_cnt)) {
		cos_meas_event(COS_MPD_SUBORDINATE);
		cos_put_pg_pool(pa_to_va((void*)slave_cspd->spd_info.pg_tbl));
		spd_mpd_subordinate(slave_cspd, master_cspd);
	} 
	/* 
	 * Here we will only decriment the count and not fully free
	 * the cspd if we subordinated the cspd .
	 */
	spd_mpd_release(slave_cspd);

	return;
}

void spd_mpd_release_desc(short int desc)
{
	assert(desc < MAX_MPD_DESC);

	spd_mpd_release(&mpd_descriptors[desc]);
}

void spd_mpd_free_all(void)
{
	int i;

	for (i = 0 ; i < MAX_MPD_DESC ; i++) {
		struct composite_spd *cspd;

		cspd = spd_mpd_by_idx(i);
		if (!(cspd->spd_info.flags & SPD_FREE)) {
			printk("cos: warning - found unfreed composite spd, %d w/ refcnt %d\n", 
			       spd_mpd_index(cspd), cos_ref_val(&cspd->spd_info.ref_cnt));
			if (!spd_mpd_is_subordinate(cspd)) {
				cos_put_pg_pool(pa_to_va((void*)cspd->spd_info.pg_tbl));
			}
			//spd_mpd_release(cspd);
		}
	}

	spd_init_mpd_descriptors();

	return;
}

/*
 * Kill the composite spd, removing its spds and setting them to use
 * their own protection domains.
 *
 * Assume that the cspd is not depricated, i.e. the current cspd for
 * its spds.
 */
static void spd_mpd_terminate(struct composite_spd *cspd)
{
	struct spd *spds;

	assert(cspd);// && !spd_mpd_is_depricated(cspd));

	spds = cspd->members;
	while (spds) {
		struct spd *next = spds->composite_member_next;
		spds->composite_spd = &spds->spd_info;
		spds->composite_member_next = NULL;
		spds->composite_member_prev = NULL;
		spds = next;
	}

	cspd->members = NULL;
	spd_mpd_depricate(cspd);
	
	return;
}

static inline int spd_in_composite(struct spd *spd, struct composite_spd *cspd)
{
	return spd_is_member(spd, cspd);
	/*
	struct spd *curr;

	curr = cspd->members;

	while (curr) {
		if (curr == spd) {
			return 1;
		}
		curr = curr->composite_member_next;
	}

	return 0;
	*/
}

/*
 * Change the isolation level going from a specific spd to any and all
 * spds in a given composite spd to il.
 */
static void spd_chg_il_spd_to_all(struct spd *spd, struct composite_spd *cspd, isolation_level_t il)
{
	unsigned short int cap_lower, cap_range;
	int i;

	/* ignore the first "return" capability */
	cap_lower = spd->cap_base + 1;
	cap_range = spd->cap_range - 1;

	//printk("cos: changing up to %d caps for spd %d\n", cap_range, spd_get_index(spd));

	for (i = cap_lower ; i < cap_lower+cap_range ; i++) {
		struct invocation_cap *cap = &invocation_capabilities[i];
		struct spd *dest = cap->destination;

		if (dest && dest != spd && spd_in_composite(dest, cspd)) {
			isolation_level_t old;

			//printk("cos:\tST from %d to %d.\n", spd_get_index(spd), spd_get_index(dest));

			old = cap_change_isolation(i, il, 0);
			assert(old != il && old != IL_INV);
		}
	}

	return;
}

/*
 * Change the isolation level going from all spds in a specific
 * composite spd to a specific spd to a given isolation level.  Used
 * to add or remove a spd from a composite.
 */
static void spd_chg_il_all_to_spd(struct composite_spd *cspd, struct spd *spd, isolation_level_t il)
{
	struct spd *curr;

	assert(spd && cspd && spd_is_composite(&cspd->spd_info));/* && 
								    !spd_mpd_is_depricated(cspd));*/

	curr = cspd->members;

	while (curr) {
		unsigned short int cap_lower, cap_range;
		int i;
		
		/* ignore the first "return" capability */
		cap_lower = curr->cap_base + 1;
		assert(curr->cap_range > 0);
		cap_range = curr->cap_range - 1;

		//printk("cos: changing up to %d caps in spd %d to spd %d\n", curr->cap_range, spd_get_index(curr), spd_get_index(spd));
		
		for (i = cap_lower ; i < cap_lower+cap_range ; i++) {
			struct invocation_cap *cap = &invocation_capabilities[i];
			struct spd *dest = cap->destination;

			if (dest && cap->owner != spd && dest == spd) {
				isolation_level_t old;

				//printk("cos:\tST from %d to %d.\n", spd_get_index(curr), spd_get_index(dest));
				
				old = cap_change_isolation(i, il, 0);
				assert(old != il && old != IL_INV);
			}
		}
		curr = curr->composite_member_next;
	}
	
	return;
}

#define MAXV (~(0UL))
unsigned long spd_read_reset_invocation_cnt(struct spd *cspd, struct spd *sspd)
{
	unsigned long tot = 0;
	unsigned int cnt = 0;
	int i, cap_lo, cap_hi;
	assert(cspd && sspd);

	cap_lo = cspd->cap_base+1;
	assert(cspd->cap_range > 0);
	cap_hi = cspd->cap_range + cap_lo - 1;

	for (i = cap_lo; i <= cap_hi; i++) {
		struct invocation_cap *cap = &invocation_capabilities[i];
		assert(cap->owner == cspd);
		if (cap->destination != sspd) continue;

		if (cap_reset_cap_inv_cnt(i, &cnt)) {
			printk("cos: Error reading capability %d from spd %d->%d.\n",
			       i, spd_get_index(cspd), spd_get_index(sspd));
			continue;
		}
		/* For overflow: */
		tot = ((tot + cnt) < tot) ? MAXV : tot + cnt;
	}
	
	return tot;
}

/*
 * assume that a test for membership of new_spd in cspd has already
 * been carried out.
 */
static inline void spd_add_caps(struct composite_spd *cspd, struct spd *new_spd)
{
	spd_chg_il_all_to_spd(cspd, new_spd, IL_ST);
	spd_chg_il_spd_to_all(new_spd, cspd, IL_ST);
}

/*
 * assume that a test for membership of new_spd in cspd has already
 * been carried out.
 */
static inline void spd_remove_caps(struct composite_spd *cspd, struct spd *spd)
{
	spd_chg_il_all_to_spd(cspd, spd, IL_SDT);
	spd_chg_il_spd_to_all(spd, cspd, IL_SDT);
}

static inline void spd_remove_mappings(struct composite_spd *cspd, struct spd *spd)
{
	phys_addr_t tbl = cspd->spd_info.pg_tbl;

	zero_pgtbl_range(tbl, spd->location.lowest_addr, spd->location.size);
}

static inline void spd_add_mappings(struct composite_spd *cspd, struct spd *spd) 
{
	copy_pgtbl_range(cspd->spd_info.pg_tbl, spd->spd_info.pg_tbl, 
			 spd->location.lowest_addr, spd->location.size);
}

/*
 * Add the spd to the composite spd.  This includes changing the
 * capability settings so that all intra-composite commnication uses
 * symmetric trust, and all communication outside uses symmetric
 * distrust.  Further, it adds the address space mappings of the spd
 * to the composite spd, and sets the spd to currently belong to this
 * composite.
 *
 * Assumes that the spd does not currently belong to another composite.
 */
int spd_composite_add_member(struct composite_spd *cspd, struct spd *spd)
{
	struct spd *next;

	assert(!spd_mpd_is_depricated(cspd));
	/* verify spd is not resident in any other composite_spd */
	assert(&spd->spd_info == spd->composite_spd);
	/* make sure spd's mappings don't already exist in cspd (a bug) */
	assert(pgtbl_entry_absent(cspd->spd_info.pg_tbl, spd->location.lowest_addr));
	
	spd_add_caps(cspd, spd);
	spd_add_mappings(cspd, spd);

	if (pgtbl_entry_absent(cspd->spd_info.pg_tbl, spd->location.lowest_addr)) {
		printk("cos: adding spd to cspd -> page tables for cspd not initialized properly.\n");
		if (pgtbl_entry_absent(spd->spd_info.pg_tbl, spd->location.lowest_addr)) {
			printk("cos: adding spd to cspd -> page tables for spd not initialized properly.\n");
		}
		return -1;
	}

	next = cspd->members;
	spd->composite_member_next = cspd->members;
	spd->composite_member_prev = NULL;
	cspd->members = spd;
	if (next) next->composite_member_prev = spd;
	spd->composite_spd = &cspd->spd_info;

	//printk("cos: spd %d added to cspd %d.\n", spd_get_index(spd), spd_mpd_index(cspd));

	return 0;
}

/*
static void print_members(struct composite_spd *cspd)
{
	struct spd *spd = cspd->members;

	printk("cos: cspd %d contains (", spd_mpd_index(cspd));
	while (spd) {
		printk("%d, ", spd_get_index(spd));
		
		spd = spd->composite_member_next;
	}
	printk(")\n");
}
*/

int spd_composite_remove_member(struct spd *spd, int remove_mappings)
{
	struct spd *prev, *next;
	struct composite_spd *cspd;

	assert(spd && spd_is_composite(spd->composite_spd));

	cspd = (struct composite_spd *)spd->composite_spd;

	//assert(!spd_mpd_is_depricated(cspd));
	/* this spd better be present in its composite's pgtbl or bug */
	assert(!pgtbl_entry_absent(cspd->spd_info.pg_tbl, spd->location.lowest_addr));

	//print_members(cspd);
	prev = spd->composite_member_prev;
	next = spd->composite_member_next;

	if (NULL == prev) {
		cspd->members = next;
		if (NULL != next) next->composite_member_prev = NULL;
	} else {
		prev->composite_member_next = next;
		if (NULL != next) next->composite_member_prev = prev;
	}

	spd->composite_member_prev = NULL;
	spd->composite_member_next = NULL;
	spd->composite_spd = &spd->spd_info;

	spd_remove_caps(cspd, spd);
	
	if (remove_mappings) {
		spd_remove_mappings(cspd, spd);
		assert(pgtbl_entry_absent(cspd->spd_info.pg_tbl, spd->location.lowest_addr));
	}

	//print_members(cspd);

	//printk("cos: spd %d removed from cspd %d.\n", spd_get_index(spd), spd_mpd_index(cspd));
	
	return 0;
}
