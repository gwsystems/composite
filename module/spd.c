/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

//#include <spd.h>
#include "include/spd.h"
#include "include/debug.h"
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

/* 
 * FIXME: access control
 */
isolation_level_t cap_change_isolation(int cap_num, isolation_level_t il, int flags)
{
	isolation_level_t prev;
	struct invocation_cap *cap;
	struct spd *owner;
	struct usr_inv_cap *ucap;

	printk("cos: change to il %s for cap %d.\n", 
	       ((il == IL_SDT) ? "SDT" : ((il == IL_AST) ? "AST" : ((il == IL_ST) ? "ST" : "INV"))), 
	       cap_num);

	if (cap_num >= MAX_STATIC_CAP) {
		return IL_INV;
	}

	cap = &invocation_capabilities[cap_num];
	prev = cap->il;
	//if (prev == il) return prev;

	cap->il = il;

	owner = cap->owner;
	ucap = &owner->user_cap_tbl[cap_num - owner->cap_base];

	if (flags & CAP_SAVE_REGS) {
		/* set highest order bit to designate saving of
		 * registers. */
		cap_num = (cap_num<<20)|0x80000000; 
	} else {
		cap_num <<= 20;
	}

	switch (il) {
	case IL_SDT:
		printk("cos:\tSDT w/ entry %x.\n", (unsigned int)cap->usr_stub_info.SD_serv_stub);
		cap->dest_entry_instruction = cap->usr_stub_info.SD_serv_stub;
		cap_set_usr_cap(ucap, cap->usr_stub_info.SD_cli_stub, 0, cap_num);
		break;
	case IL_AST:
		printk("cos:\tAST w/ entry %x.\n", (unsigned int)cap->usr_stub_info.AT_serv_stub);
		cap->dest_entry_instruction = cap->usr_stub_info.AT_serv_stub;
		cap_set_usr_cap(ucap, cap->usr_stub_info.AT_cli_stub, 0, cap_num);
		break;
	case IL_ST:
		printk("cos:\tST w/ entry %x.\n", (unsigned int)cap->usr_stub_info.ST_serv_entry);
		cap->dest_entry_instruction = cap->usr_stub_info.ST_serv_entry;
		cap_set_usr_cap(ucap, cap->usr_stub_info.ST_serv_entry, 0, cap_num);
		break;
	default:
		return IL_INV;
	}

	return prev;
}

static struct spd spds[MAX_NUM_SPDS];

/*
 * This should be a per-cpu data structure and should be protected
 * with int_(enable|disable).
 */
static struct spd *spd_freelist_head = NULL;

static void spd_init_all(struct spd *spds)
{
	int i;
	
	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		spds[i].spd_info.flags |= SPD_FREE;
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
void spd_free(struct spd *spd)
{
	//spd_free_mm(spd);
	spd->spd_info.flags |= SPD_FREE;
	spd->freelist_next = spd_freelist_head;
	spd_freelist_head = spd;
	if (spd->composite_spd != &spd->spd_info) {
		spd_mpd_release((struct composite_spd*)spd->composite_spd);
	}

	return;
}

extern int spd_free_mm(struct spd *spd);
void spd_mpd_free_all(void);
void spd_free_all(void)
{
	int i;
	
	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		if (!(spds[i].spd_info.flags |= SPD_FREE)) {
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
	spd->spd_info.flags &= ~SPD_FREE;

	/* +1 for the (dummy) 0th return cap */
	ret = spd_alloc_capability_range(num_caps+1);

	if (ret == -1) {
		printd("cos: could not allocate a capability range.\n");
		goto free_spd;
	}
	
	spd->cap_base = (unsigned short int)ret;
	spd->cap_range = num_caps+1;

	spd->user_cap_tbl = user_cap_tbl;
	spd->upcall_entry = upcall_entry;
	spd->sched_shared_page = NULL;
	
	/* This will cause the spd to never be deallocated via garbage collection */
	spd->spd_info.ref_cnt.counter = 2; 

	/* return capability; ignore return value as we know it will be 0 */
	spd_add_static_cap(spd, 0, spd, 0);

	spd->composite_spd = &spd->spd_info;

	spd->sched_depth = -1;
	spd->parent_sched = NULL;


	printk("cos: allocated spd %p\n", spd);

	return spd;

 free_spd:
	spd_free(spd);
	return NULL;
}

int spd_get_index(struct spd *spd)
{
	return ((unsigned long)spd-(unsigned long)spds)/sizeof(struct spd);
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

	if (!owner_spd || !trusted_spd || owner_spd->user_cap_tbl == NULL) {
		printd("cos: Invalid cap request args (%p, %p, %x).\n",
		       owner_spd, trusted_spd, (unsigned int)owner_spd->user_cap_tbl);
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

extern void *cos_alloc_page(void);
extern void *va_to_pa(void *va);
extern void *pa_to_va(void *pa);

struct composite_spd mpd_descriptors[MAX_MPD_DESC];
struct composite_spd *mpd_freelist;

/* we're going to have a page-pool as well */
struct page_list {
	struct page_list *next;
} page_list_head;
unsigned int page_list_len = 0;

extern vaddr_t kern_pgtbl_mapping;
extern void zero_pgtbl_range(phys_addr_t pt, unsigned long lower_addr, unsigned long size);
extern void copy_pgtbl_range(phys_addr_t pt_to, phys_addr_t pt_from,
			     unsigned long lower_addr, unsigned long size);

static struct page_list *cos_get_pg_pool(void)
{
	struct page_list *page;

	/*
	 * If we ran out of pages in our cache, allocate another, and
	 * copy the kernel page table mappings into it, otherwise,
	 * take a page out of our cache.
	 */
	if (NULL == page_list_head.next) {
		page = cos_alloc_page();
		if (NULL == page) return NULL;

		copy_pgtbl_range((phys_addr_t)va_to_pa(page), 
				 (phys_addr_t)va_to_pa((void*)kern_pgtbl_mapping),
				 0, 0xFFFFFFFF);
	} else {
		page = page_list_head.next;
		page_list_head.next = page->next;
		page_list_len--;
		page->next = NULL;
	}

	return page;
}

static void cos_put_pg_pool(struct page_list *page)
{
	page->next = page_list_head.next;
	page_list_head.next = page;
	page_list_len++;

	/* arbitary test, but this is an error case I would like to be able to catch */
	assert(page_list_len < 1024);

	return;
}

void spd_init_mpd_descriptors(void)
{
	int i;
	struct page_list *page;

	mpd_freelist = mpd_descriptors;
	for (i = 0 ; i < MAX_MPD_DESC ; i++) {
		struct composite_spd *cspd = &mpd_descriptors[i];

		cspd->spd_info.flags = SPD_COMPOSITE | SPD_FREE;
		cspd->spd_info.ref_cnt.counter = 0;
		cspd->freelist_next = &mpd_descriptors[i+1];
		cspd->members = NULL;
	}
	mpd_descriptors[MAX_MPD_DESC-1].freelist_next = NULL;

	/* put one page into the page pool */
	page = cos_get_pg_pool();
	assert(NULL != page);
	cos_put_pg_pool(page);

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

short int spd_alloc_mpd_desc(void)
{
	struct composite_spd *new;
	struct page_list *page;

	new = mpd_freelist;
	if (NULL == new) 
		return -1;

	assert((new->spd_info.flags & (SPD_FREE | SPD_COMPOSITE)) == (SPD_FREE | SPD_COMPOSITE));
	assert(0 == new->spd_info.ref_cnt.counter);
	assert(NULL == new->members);

	mpd_freelist = new->freelist_next;
	new->freelist_next = NULL;
	new->members = NULL;
	new->spd_info.flags &= ~SPD_FREE;

	page = cos_get_pg_pool();
	if (NULL == page)
		return -1;

	/* FIXME: make really atomic...not necessary here, but for cleanliness */
	new->spd_info.ref_cnt.counter = 1;
	new->spd_info.pg_tbl = (phys_addr_t)va_to_pa(page);

	return spd_mpd_index(new);
}

void spd_mpd_release(struct composite_spd *cspd)
{
	/* FIXME: again, should be atomic */
	cspd->spd_info.ref_cnt.counter--;
	cspd->members = NULL;

	if (0 == cspd->spd_info.ref_cnt.counter) {
		cspd->spd_info.flags |= SPD_FREE;
		cspd->freelist_next = mpd_freelist;
		mpd_freelist = cspd;
		cos_put_pg_pool(pa_to_va((void*)cspd->spd_info.pg_tbl));
	}

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
		if (!(cspd->spd_info.flags & SPD_FREE)) 
			spd_mpd_release(cspd);
	}

	spd_init_mpd_descriptors();

	return;
}

static inline int spd_in_composite(struct spd *spd, struct composite_spd *cspd)
{
	struct spd *curr;

	curr = cspd->members;

	while (curr) {
		if (curr == spd) {
			return 1;
		}
		curr = curr->composite_member_next;
	}

	return 0;
}

/*
 * Change the isolation level going from a specific spd to any and all
 * spds in a given composite spd to il.
 */
static void spd_chg_il_spd_to_all(struct spd *spd, struct composite_spd *cspd, isolation_level_t il)
{
	unsigned short int cap_lower, cap_range;
	int i;

	cap_lower = spd->cap_base;
	cap_range = spd->cap_range;

	for (i = cap_lower ; i < cap_lower+cap_range ; i++) {
		struct invocation_cap *cap = &invocation_capabilities[i];
		struct spd *dest = cap->destination;

		if (spd_in_composite(dest, cspd)) {
			isolation_level_t old;

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

	curr = cspd->members;

	while (curr) {
		unsigned short int cap_lower, cap_range;
		int i;
		
		cap_lower = spd->cap_base;
		cap_range = spd->cap_range;

		for (i = cap_lower ; i < cap_lower+cap_range ; i++) {
			struct invocation_cap *cap = &invocation_capabilities[i];
			struct spd *dest = cap->destination;

			if (dest == spd) {
				isolation_level_t old;
				
				old = cap_change_isolation(i, il, 0);
				assert(old != il && old != IL_INV);
			}
		}

		curr = curr->composite_member_next;
	}
	
	return;
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
	/*
	 * If the spd to be added is still part of another composite
	 * spd, then we cannot add it to the current spd.
	 */
	if (&spd->spd_info != spd->composite_spd) {
		return -1;
	}

	spd_add_caps(cspd, spd);
	spd_add_mappings(cspd, spd);

	spd->composite_member_next = cspd->members;
	spd->composite_member_prev = NULL;
	cspd->members = spd;
	spd->composite_spd = &cspd->spd_info;

	return 0;
}

int spd_composite_remove_member(struct composite_spd *cspd, struct spd *spd, int remove_mappings)
{
	struct spd *prev, *next;

	assert(!spd_mpd_is_depricated(cspd));

	/*
	 * A spd that is not part of a composite spd cannot be removed
	 * from it...
	 */
	if (spd->composite_spd != &cspd->spd_info) {
		return -1;
	}

	prev = spd->composite_member_prev;
	next = spd->composite_member_next;

	if (NULL == prev) {
		cspd->members = next;
	} else {
		prev->composite_member_next = next;
		if (NULL != next) next->composite_member_prev = prev;
	}

	spd->composite_member_prev = NULL;
	spd->composite_member_next = NULL;
	spd->composite_spd = &spd->spd_info;

	spd_remove_caps(cspd, spd);
	
	if (remove_mappings)
		spd_remove_mappings(cspd, spd);

	return 0;
}

