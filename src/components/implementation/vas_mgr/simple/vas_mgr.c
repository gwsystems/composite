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

vaddr_t vas_mgr_expand(spdid_t spd, long amnt)
{
	struct spd_vas_info *svi;
	vaddr_t ret = 0;
	int loc;
	unsigned long a, i, nentries;

	amnt = round_up_to_pgd_page(amnt); 
	LOCK();
	svi = cos_vect_lookup(&spd_vas_map, spd);
	if (!svi) {
		svi = malloc(sizeof(struct spd_vas_info));
		if (!svi) goto done;
		memset(svi, 0, sizeof(struct spd_vas_info));
		svi->spdid = spd;
		svi->vas = vas;
		if (-1 == cos_vect_add_id(&spd_vas_map, svi, spd)) {
			free(svi);
			goto done;
		}
	}

	for (i = 0 ; i < MAX_SPD_VAS_LOCATIONS ; i++) {
		if (!svi->locations[i].size) break;
	}
	if (i == MAX_SPD_VAS_LOCATIONS) goto done;

	loc = i;
	nentries = amnt>>PGD_SHIFT;
	for (a = SERVICE_START, i = SERVICE_START>>PGD_SHIFT ; 
	     a < (SERVICE_END-amnt) ; 
	     a += SERVICE_SIZE, i++) {
		unsigned long s = a, s_idx = i;
		int found = 1;

		printc("i=%ld, %ld\n", i, nentries);

		for (; i < (s_idx + nentries) ; a += SERVICE_SIZE, i++) {
			if (vas->s[i]) {
				found = 0;
				break;
			}
			printc("a\n");
		}
		printc("found %d\n", found);
		a -= SERVICE_SIZE;
		i--;
		if (!found) continue;
		
		if (cos_vas_cntl(COS_VAS_SPD_EXPAND, spd, s, amnt)) {
			vas->s[s_idx] = &unknown;
			continue;
		}
		/* success */
		for (i = s_idx ; i < (s_idx + nentries) ; i++) vas->s[i] = svi;
		svi->locations[loc].size = amnt;
		svi->locations[loc].base = ret = s;
		break;
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
