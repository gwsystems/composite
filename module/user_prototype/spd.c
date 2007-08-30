/* 
 * Author: Gabriel Parmer
 * License: GPLv2
 */

#include <spd.h>

#include <stdio.h>
#include <malloc.h>

struct invocation_cap invocation_capabilities[MAX_STATIC_CAP];

static void spd_init_capabilities(struct invocation_cap *caps)
{
	int i;

	for (i = 0 ; i < MAX_STATIC_CAP ; i++) {
		caps[i].owner = caps[i].destination = MNULL;
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
		struct invocation_cap *cap = &invocation_capabilities[i];

		if (cap->owner == MNULL) {
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
					init->owner = (void*)1;
				}

				return i-range_free+1;
			}
		} else {
			range_free = 0;
		}
	}

	return -1;
}

static struct spd spds[MAX_NUM_SPDS];

/*
 * This should be a per-cpu data structure and should be protected
 * with int_(enable|disable).
 */
static struct spd *spd_freelist_head = MNULL;

static void spd_init_all(struct spd *spds)
{
	int i;
	
	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		spds[i].spd_info.flags |= SPD_FREE;
		spds[i].freelist_next = (i == (MAX_NUM_SPDS-1)) ? MNULL : &spds[i+1];
	}

	spd_freelist_head = spds;

	return;
}

void spd_init(void)
{
	spd_init_all(spds);
	spd_init_capabilities(invocation_capabilities);
}

void spd_free(struct spd *spd)
{
	spd->spd_info.flags |= SPD_FREE;
	spd->freelist_next = spd_freelist_head;
	spd_freelist_head = spd;

	return;
}

struct spd *spd_alloc(unsigned short int num_caps, struct usr_inv_cap *user_cap_tbl)
{
	struct spd *spd;
	int ret;

	spd = spd_freelist_head;

	if (spd == MNULL)
		return MNULL;

	/* remove from freelist */
	spd_freelist_head = spd_freelist_head->freelist_next;
	spd->spd_info.flags &= ~SPD_FREE;

	/* +1 for the (dummy) 0th return cap */
	ret = spd_alloc_capability_range(num_caps+1);

	if (ret == -1) {
		goto free_spd;
	}
	
	spd->cap_base = (unsigned short int)ret;
	spd->cap_range = num_caps+1;
	spd->cap_used = 0;

	spd->user_cap_tbl = user_cap_tbl;

	/* return capability; ignore return value as we know it will be 0 */
	spd_add_static_cap(spd, 0, spd, 0);

	spd->composite_spd = (struct composite_spd*)spd;

	return spd;

 free_spd:
	spd_free(spd);
	return MNULL;
}

/* 
 * Static Capability Manipulation Functions
 */

extern vaddr_t ipc_client_marshal;

/*
 * Return the capability number allocated, or 0 on error (too many
 * static capabilities allocated).
 */
unsigned int spd_add_static_cap(struct spd *owner_spd, vaddr_t service_entry_inst, 
				struct spd *trusted_spd, isolation_level_t isolation_level)
{
	struct invocation_cap *new_cap;
	int cap_num, usr_cap_num;

	if (!owner_spd || !trusted_spd || owner_spd->user_cap_tbl == MNULL) {
		return 0;
	}

	/*
	 * static capabilities cannot be added past the hard limit set
	 * at spd allocation time.
	 */
	if (owner_spd->cap_used >= owner_spd->cap_range ||
	    isolation_level > MAX_ISOLATION_LVL_VAL)
		return 0;

	cap_num = owner_spd->cap_base + owner_spd->cap_used;
	usr_cap_num = owner_spd->cap_used;
	new_cap = &invocation_capabilities[cap_num];
	owner_spd->cap_used++;

	/* initialize the new capability's information */
	new_cap->dest_entry_instruction = service_entry_inst;
	new_cap->owner = owner_spd;
	new_cap->destination = trusted_spd;
	new_cap->invocation_cnt = 0;
	new_cap->il = isolation_level;
	
	/* and user-level representation (touching user-level pages here) */
	owner_spd->user_cap_tbl[usr_cap_num].invocation_fn = service_entry_inst;//0;//(vaddr_t)&ipc_client_marshal;
	owner_spd->user_cap_tbl[usr_cap_num].service_entry_inst = service_entry_inst;
	owner_spd->user_cap_tbl[usr_cap_num].invocation_count = 0;
	owner_spd->user_cap_tbl[usr_cap_num].cap_no = cap_num;

	return cap_num;
}
