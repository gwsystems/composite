/**
 * Copyright 2011 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2011
 */

#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <cos_alloc.h>
#include <cos_vect.h>
#include <vas_mgr.h>
#include <valloc.h>
#include <cinfo.h>
#include <bitmap.h>

#define LOCK_COMPONENT
#ifdef LOCK_COMPONENT
#include <cos_synchronization.h>
cos_lock_t valloc_lock;
#define LOCK()      do { if (lock_take(&valloc_lock))    BUG(); } while(0);
#define UNLOCK()    do { if (lock_release(&valloc_lock)) BUG(); } while(0);
#define LOCK_INIT() lock_static_init(&valloc_lock);
#else
#define LOCK()   if (sched_component_take(cos_spd_id())) BUG();
#define UNLOCK() if (sched_component_release(cos_spd_id())) BUG();
#define LOCK_INIT()
#endif

/* vector of vas vectors for spds */
COS_VECT_CREATE_STATIC(spd_vect);

#define WORDS_PER_PAGE (PAGE_SIZE/sizeof(u32_t))
#define MAP_MAX WORDS_PER_PAGE
#define VAS_SPAN (WORDS_PER_PAGE * sizeof(u32_t) * 8)
#define EXTENT_SIZE (32 * 1024 * 1024 / PAGE_SIZE)

/* describes 2^(12+12+3 = 27) bytes */
struct spd_vas_occupied {
	u32_t pgd_occupied[WORDS_PER_PAGE];
};

/* #if sizeof(struct spd_vas_occupied) != PAGE_SIZE */
/* #error "spd_vas_occupied not sized to a page" */
/* #endif */

struct vas_extent {
	void *start, *end;
	struct spd_vas_occupied *map;
};

struct spd_vas_tracker {
	spdid_t spdid;
	struct vas_extent extents[MAX_SPD_VAS_LOCATIONS];
	/* should be an array to track more than 2^27 bytes */
	struct spd_vas_occupied *map; 
};

int valloc_fork_spd(spdid_t spdid, spdid_t o_spdid, spdid_t f_spdid) 
{
	int ret = -1;
	struct spd_vas_tracker *f_trac, *o_trac;
	struct spd_vas_occupied *f_occ;
	unsigned long page_off;
	void *o_hp;
	int i;

	if (!cos_vect_lookup(&spd_vect, o_spdid)) goto done;
	o_trac = cos_vect_lookup(&spd_vect, o_spdid);
	if (!o_trac) goto done;
	f_trac = malloc(sizeof(struct spd_vas_tracker));
	if (!f_trac) goto done;

	f_occ = alloc_page();
	if (!f_occ) goto err_free1;
	
	o_hp = cinfo_get_heap_pointer(cos_spd_id(), f_spdid);
	if (!o_hp) goto err_free2;

        f_trac->spdid            = f_spdid;
        f_trac->map              = f_occ;

	for (i = 0; i < MAX_SPD_VAS_LOCATIONS; i++) {
		if (o_trac->extents[i].start != 0) {		/* naively simple way to make sure we don't copy empty tracs */
		        f_trac->extents[i].start = o_trac->extents[i].start;
        		f_trac->extents[i].end   = o_trac->extents[i].end;
        		f_trac->extents[i].map   = o_trac->extents[i].map;
		}
	}
        page_off = ((unsigned long)o_hp - (unsigned long)round_to_pgd_page(o_hp))/PAGE_SIZE; // also not
        bitmap_set_contig(&f_occ->pgd_occupied[0], page_off, (PGD_SIZE/PAGE_SIZE)-page_off, 1);
        bitmap_set_contig(&f_occ->pgd_occupied[0], 0, page_off, 0);

	cos_vect_add_id(&spd_vect, f_trac, f_spdid);
	assert(cos_vect_lookup(&spd_vect, f_spdid));
success:
	ret = 0;
done:
	return ret;
err_free2:
	free_page(f_occ);
err_free1:
	free(f_trac);
	goto done;
}

static int __valloc_init(spdid_t spdid)
{
	int ret = -1;
	struct spd_vas_tracker *trac;
	struct spd_vas_occupied *occ;
	unsigned long page_off;
	void *hp;

	if (cos_vect_lookup(&spd_vect, spdid)) goto success;
	trac = malloc(sizeof(struct spd_vas_tracker));
	if (!trac) goto done;

	occ = alloc_page();
	if (!occ) goto err_free1;
	
	hp = cinfo_get_heap_pointer(cos_spd_id(), spdid);
	if (!hp) goto err_free2;

        trac->spdid            = spdid;
        trac->map              = occ;
        trac->extents[0].start = (void*)round_to_pgd_page(hp);
        trac->extents[0].end   = (void*)round_up_to_pgd_page(hp);
        trac->extents[0].map   = occ;
        page_off = ((unsigned long)hp - (unsigned long)round_to_pgd_page(hp))/PAGE_SIZE;
        bitmap_set_contig(&occ->pgd_occupied[0], page_off, (PGD_SIZE/PAGE_SIZE)-page_off, 1);
        bitmap_set_contig(&occ->pgd_occupied[0], 0, page_off, 0);

	cos_vect_add_id(&spd_vect, trac, spdid);
	assert(cos_vect_lookup(&spd_vect, spdid));
success:
	ret = 0;
done:
	return ret;
err_free2:
	free_page(occ);
err_free1:
	free(trac);
	goto done;
}


int valloc_alloc_at(spdid_t spdid, spdid_t dest, void *addr, unsigned long npages)
{
	int ret = -1, i = 0;
	struct spd_vas_tracker *trac;
	struct spd_vas_occupied *occ;
	unsigned long off, ext_size;

	LOCK();
	trac = cos_vect_lookup(&spd_vect, dest);
	if (!trac) {
		if (__valloc_init(dest) ||
		    !(trac = cos_vect_lookup(&spd_vect, dest))) goto done;
	}

	if (unlikely(npages > MAP_MAX * 32)) {
		printc("valloc: cannot alloc more than %u bytes in one time!\n", 32 * WORDS_PER_PAGE * PAGE_SIZE);
		goto done;
	}

	while (trac->extents[i].map) {
		if (addr < trac->extents[i].start || addr > trac->extents[i].end) {
			if (++i == MAX_SPD_VAS_LOCATIONS) goto done;
			continue;
		}
		/* the address is in the range of an existing extent */
		occ = trac->extents[i].map;
		off = ((char*)addr - (char*)trac->extents[i].start) / PAGE_SIZE;
		assert(off + npages < MAP_MAX * 32);
		ret = bitmap_extent_set_at(&occ->pgd_occupied[0], off, npages, MAP_MAX);
		goto done;
	}

	ext_size = round_up_to_pgd_page(npages * PAGE_SIZE);
	trac->extents[i].map = alloc_page();
	occ = trac->extents[i].map;
	assert(occ);
	if (vas_mgr_take(spdid, dest, (vaddr_t)addr, ext_size) == 0) goto free;
	trac->extents[i].start = addr;
	trac->extents[i].end = (void *)((uintptr_t)addr + ext_size);
	bitmap_set_contig(&occ->pgd_occupied[0], 0, ext_size / PAGE_SIZE, 1);
	bitmap_set_contig(&occ->pgd_occupied[0], 0, npages, 0);
	ret = 0;
done:
	UNLOCK();
	return ret;
free:
	free_page(trac->extents[i].map);
	goto done;
}

void *valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages)
{
	void *ret = NULL;
	struct spd_vas_tracker *trac;
	struct spd_vas_occupied *occ;
	unsigned long ext_size;
	long off, i = 0;

	LOCK();

	trac = cos_vect_lookup(&spd_vect, dest);
	if (!trac) {
		if (__valloc_init(dest) ||
		    !(trac = cos_vect_lookup(&spd_vect, dest))) goto done;
	}

	if (unlikely(npages > MAP_MAX * 32)) {
		printc("valloc: cannot alloc more than %u bytes in one time!\n", 32 * WORDS_PER_PAGE * PAGE_SIZE);
		goto done;
	}

	while (trac->extents[i].map) {
		occ = trac->extents[i].map;
		off = bitmap_extent_find_set(&occ->pgd_occupied[0], 0, npages, MAP_MAX);
		if (off < 0) {
			if (++i == MAX_SPD_VAS_LOCATIONS) goto done;
			continue;
		}
		ret = (void *)((char *)trac->extents[i].start + off * PAGE_SIZE);
		goto done;
	}

	ext_size = round_up_to_pow2(npages, PGD_SIZE/PAGE_SIZE);
	trac->extents[i].map = alloc_page();
	occ = trac->extents[i].map;
	assert(occ);
	trac->extents[i].start = (void*)vas_mgr_expand(spdid, dest, ext_size * PAGE_SIZE);
	trac->extents[i].end = (void *)(trac->extents[i].start + ext_size * PAGE_SIZE);
	bitmap_set_contig(&occ->pgd_occupied[0], 0, ext_size, 1);
	bitmap_set_contig(&occ->pgd_occupied[0], 0, npages, 0);
	ret = trac->extents[i].start;
done:
	UNLOCK();
	return ret;
}

int valloc_free(spdid_t spdid, spdid_t dest, void *addr, unsigned long npages)
{
	int ret = -1;
	struct spd_vas_tracker *trac;
	struct spd_vas_occupied *occ;
	unsigned long off;

	LOCK();
	trac = cos_vect_lookup(&spd_vect, dest);
	if (!trac) goto done;

	int i = 0;
	/* locate the address to be freed in which range (extents) */
	while (addr < trac->extents[i].start || addr >= trac->extents[i].end) {
		if (++i == MAX_SPD_VAS_LOCATIONS) goto done;
	}
	occ = trac->extents[i].map;
	assert(occ);
	off = ((char *)addr - (char *)trac->extents[i].start) / PAGE_SIZE;
	assert(off + npages < MAP_MAX * 32);
	bitmap_set_contig(&occ->pgd_occupied[0], off, npages, 1);
	ret = 0;
done:	
	UNLOCK();
	return ret;
}

static void init(void)
{
	LOCK_INIT();
	cos_vect_init_static(&spd_vect);
}

void cos_init(void *arg)
{
	static volatile int first = 1;

	if (first) {
		first = 0;
		init();
	} else {
		prints("vas_mgr: not expecting more than one bootstrap.");
	}
}
