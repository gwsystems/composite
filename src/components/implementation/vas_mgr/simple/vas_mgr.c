/**
 * Copyright 2010 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2010
 */

#include <cos_component.h>
#include <cos_synchronization.h>
#include <cos_alloc.h>
#include <cos_vect.h>
#include <vas_mgr.h>
#include <sched.h>

COS_VECT_CREATE_STATIC(spd_vas_map);
cos_lock_t vas_lock;
#define LOCK()   lock_take(&vas_lock)
#define UNLOCK() lock_release(&vas_lock)

struct vas_desc;
struct spd_vas_info {
	spdid_t spdid;
	struct vas_desc *vas;
	struct vas_location {
		vaddr_t base;
		long size;
	} locations[MAX_SPD_VAS_LOCATIONS];
};

struct vas_desc {
	struct spd_vas_info *(s[PGD_PER_PTBL]);
} *vas;

/* Till we have complete information when this component starts about
 * which virtual addresses are taken, we will keep this around. */
struct spd_vas_info unknown = {.spdid = 0, };

static inline struct spd_vas_info*
vas_get_svi(spdid_t spd)
{
	struct spd_vas_info *svi = cos_vect_lookup(&spd_vas_map, spd);
	if (svi) goto done;
	svi = malloc(sizeof(struct spd_vas_info));
	if (!svi) goto done;
	memset(svi, 0, sizeof(struct spd_vas_info));
	svi->spdid = spd;
	svi->vas = vas;
	if (-1 == cos_vect_add_id(&spd_vas_map, svi, spd)) {
		free(svi);
		svi = NULL;
	}
done:
	return svi;
}

static inline int
vas_first_free_svi_location(struct spd_vas_info *svi)
{
	int i;
	for (i = 0; i < MAX_SPD_VAS_LOCATIONS; i++) {
		if (!svi->locations[i].size) return i;
	}
	return -1;
}

static inline vaddr_t
__vas_mgr_expand(spdid_t spd, spdid_t dest, unsigned long start,
		unsigned long end, unsigned long size, unsigned long amnt,
		int nentries, unsigned long *ret_idx)
{
	unsigned long a, i;
	for (a = start, i = start >> PGD_SHIFT; a < end; a += size, i++) {
		unsigned long s = a, s_idx = i;
		int found = 1;

		for (; i < (s_idx + nentries) ; a += size, i++) {
			if (vas->s[i]) {
				found = 0;
				break;
			}
		}

		if (!found) continue;
		
		if (cos_vas_cntl(COS_VAS_SPD_EXPAND, dest, s, amnt)) {
			vas->s[s_idx] = &unknown;
			continue;
		}

		/* success */
		*ret_idx = s_idx;
		return s;
	}
	return 0;
}

vaddr_t
vas_mgr_expand(spdid_t spd, spdid_t dest, unsigned long amnt)
{
	struct spd_vas_info *svi;
	vaddr_t ret = 0;
	int loc;
	unsigned long s_idx, i, nentries;
	unsigned long start, end, size;

	/* add a 16M offset to not touch the dangerous zone (or use const?) */
	unsigned long SERVICE_START_SAFE = SERVICE_START + (1 << 24);

	amnt = round_up_to_pgd_page(amnt);
	LOCK();
	svi = vas_get_svi(dest);
	if (!svi) goto done;
	if ((loc = vas_first_free_svi_location(svi)) < 0) goto done;

	nentries = amnt >> PGD_SHIFT;
	start = SERVICE_START_SAFE;
	end = SERVICE_END - amnt;
	size = SERVICE_SIZE;
	ret = __vas_mgr_expand(spd, dest, start, end, size, amnt, nentries, &s_idx);
	if (ret != 0) {
		for (i = s_idx ; i < (s_idx + nentries); i++) vas->s[i] = svi;
		svi->locations[loc].size = amnt;
		svi->locations[loc].base = ret;
	}
done:
	UNLOCK();
	return ret;
}

void vas_mgr_contract(spdid_t spd, vaddr_t addr)
{
	/* -NOTSUP */
	BUG();
}

int
vas_mgr_take(spdid_t spd, spdid_t dest, vaddr_t d_addr, unsigned long amnt)
{
	struct spd_vas_info *svi;
	vaddr_t ret = 0;
	int loc;
	unsigned long s_idx, i, nentries;
	unsigned long start, end, size;

	amnt = round_up_to_pgd_page(amnt);
	LOCK();
	svi = vas_get_svi(dest);
	if (!svi) goto done;
	if ((loc = vas_first_free_svi_location(svi)) < 0) goto done;

	nentries = amnt >> PGD_SHIFT;
	start = (unsigned long)d_addr;
	end = start + amnt;
	size = amnt;
	ret = __vas_mgr_expand(spd, dest, start, end, size, amnt, nentries, &s_idx);
	if (ret != 0) {
		for (i = s_idx ; i < (s_idx + nentries); i++) vas->s[i] = svi;
		svi->locations[loc].size = amnt;
		svi->locations[loc].base = ret;
	}
done:
	UNLOCK();
	return ret;
}

static void init(void)
{
	int i;

	lock_static_init(&vas_lock);
	cos_vect_init_static(&spd_vas_map);
	vas = alloc_page();
	assert(vas);
	for (i = 0 ; i < PGD_PER_PTBL ; i++) vas->s[i] = NULL;
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
