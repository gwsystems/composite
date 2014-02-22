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
	struct cos_component_information *ci;
	struct vas_extent extents[MAX_SPD_VAS_LOCATIONS];
	/* should be an array to track more than 2^27 bytes */
	struct spd_vas_occupied *map; 
};

static int __valloc_init(spdid_t spdid)
{
	int ret = -1;
	struct spd_vas_tracker *trac;
	struct spd_vas_occupied *occ;
	struct cos_component_information *ci;
	unsigned long page_off;
	void *hp;

	if (cos_vect_lookup(&spd_vect, spdid)) goto success;
	trac = malloc(sizeof(struct spd_vas_tracker));
	if (!trac) goto done;

	occ = alloc_page();
	if (!occ) goto err_free1;
	
	ci = cos_get_vas_page();
	if (cinfo_map(cos_spd_id(), (vaddr_t)ci, spdid)) goto err_free2;
	hp = (void*)ci->cos_heap_ptr;

        trac->spdid            = spdid;
        trac->ci               = ci;
        trac->map              = occ;
        trac->extents[0].start = (void*)round_to_pgd_page(hp);
        trac->extents[0].end   = (void*)round_up_to_pgd_page(hp);
        trac->extents[0].map   = occ;
        page_off = ((unsigned long)hp - (unsigned long)round_to_pgd_page(hp))/PAGE_SIZE;
        bitmap_set_contig(&occ->pgd_occupied[0], page_off, (PGD_SIZE/PAGE_SIZE)-page_off, 1);

	cos_vect_add_id(&spd_vect, trac, spdid);
	assert(cos_vect_lookup(&spd_vect, spdid));
success:
	ret = 0;
done:
	return ret;
err_free2:
	cos_release_vas_page(ci);
	free_page(occ);
err_free1:
	free(trac);
	goto done;
}

void *valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages)
{
	void *ret = NULL;
	struct spd_vas_tracker *trac;
	struct spd_vas_occupied *occ;
	long off;

	LOCK();

	trac = cos_vect_lookup(&spd_vect, dest);
	if (!trac) {
		if (__valloc_init(dest) ||
		    !(trac = cos_vect_lookup(&spd_vect, dest))) goto done;
	}

        if (unlikely(npages > MAP_MAX * sizeof(u32_t))) {
                printc("valloc: cannot alloc more than %lu bytes in one time!\n", 32 * WORDS_PER_PAGE * PAGE_SIZE);
                goto done;
        }

        unsigned long ext_size, i;
        for (i = 0; i < MAX_SPD_VAS_LOCATIONS; i++) {
                if (trac->extents[i].map) {
                        occ = trac->extents[i].map;
                        ext_size = (trac->extents[i].end - trac->extents[i].start) / PAGE_SIZE;
                        off = bitmap_extent_find_set(&occ->pgd_occupied[0], 0, npages, MAP_MAX);
                        if (off < 0) continue;
                        ret = (void *)((char *)trac->extents[i].start + off * PAGE_SIZE);
                        goto done;
                }

                if (npages > EXTENT_SIZE) ext_size = npages;
                trac->extents[i].map = alloc_page();
                occ = trac->extents[i].map;
                assert(occ);
                trac->extents[i].start = (void*)vas_mgr_expand(spdid, dest, ext_size * PAGE_SIZE);
                trac->extents[i].end = (void *)(trac->extents[i].start + ext_size * PAGE_SIZE);
                bitmap_set_contig(&occ->pgd_occupied[0], 0, ext_size, 1);
                bitmap_set_contig(&occ->pgd_occupied[0], 0, npages, 0);
                ret = trac->extents[i].start;
                break;
        }

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

	int i;
        for (i = 0; i < MAX_SPD_VAS_LOCATIONS; i++) {
                if (addr < trac->extents[i].start || addr > trac->extents[i].end) continue; /* locate the address to be freed in which range (extents) */
                occ = trac->extents[i].map;
                assert(occ);
                off = ((char *)addr - (char *)trac->extents[i].start) / PAGE_SIZE;
                assert(off + npages < MAP_MAX * sizeof(u32_t));
                bitmap_set_contig(&occ->pgd_occupied[0], off, npages, 1);
                ret = 0;
                goto done;
        }
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
