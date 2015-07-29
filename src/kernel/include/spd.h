/**
 * Copyright 2007 by Boston University.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Initial Author: Gabriel Parmer, gabep1@cs.bu.edu, 2007.
 */

#ifndef SPD_H
#define SPD_H

#ifndef ASM

#include "shared/cos_types.h"
#include "shared/consts.h"
#include "chal.h"

/**
 * Service Protection Domains
 *
 * Protection domains for different services or components do not
 * consist of entire address spaces defined by hardware page tables.
 * Instead, a single address space is divided between all of the
 * services within an application's execution domain.  In a sense, an
 * application and all its services exist in a private single address
 * space.  However, this is not a single address space OS as ALL
 * services across the entire system for all applications exist in the
 * single address space.  
 *
 * This file defines the structures and values for defining service
 * protection domains.  
 */

/*
 * Static capabilities:
 * 
 * The user-level version of the static capability. A vector of these
 * is mapped into each spd user-level address space.  It contains the
 * address of the function to invoke to make the ipc, and the address
 * of the function in the trusted service to call, a count of the
 * number of times invocations have been made with this capability,
 * and the capability number for this entry.  Be sure to keep this
 * synchronized with asm_ipc_defs.h.
 */
/* user half found in cos_types.h */

/* ST stubs are linked automatically */
struct usr_cap_stubs {
	vaddr_t ST_serv_entry;
	vaddr_t SD_cli_stub, SD_serv_stub;
	vaddr_t AT_cli_stub, AT_serv_stub;
};

#define CAP_FREE NULL
#define CAP_ALLOCATED_UNUSED ((void*)1)

/* 
 * Structure defining the information contained in a static capability
 * for invocation with trust.
 */
union fork_counter {
	struct {
		u8_t snd;
		u8_t rcv;
	} cnt;
	u16_t origin;
};
struct spd;
struct invocation_cap {
	/* the spd that invocations are made to. destination ==
	 * NULL means that this entry is free and not in use.  */
	struct spd *destination;
	unsigned int invocation_cnt:30;
	isolation_level_t il:2;
	vaddr_t dest_entry_instruction;

	union fork_counter fork;
	/* 
	 * For now, this can be part of the structure as the structure
	 * should still remain <= 32 bytes, however if this changes,
	 * this should be removed into an array of user_cap_stubs.
	 * They are not used on the IPC path. 
	 */
	struct usr_cap_stubs usr_stub_info;
} CACHE_ALIGNED;


/* end static capabilities */

/* async invocation cap */
struct async_cap {
	// client acap
	int id;
	int srv_spd_id;
	int srv_acap_id;
	long cpu;

	int owner_thd;
	int allocated;

	// server acap
	/* int id; */
	/* int srv_spd_id; */
	int upcall_thd;
	unsigned int pending_upcall;
} CACHE_ALIGNED;

/*
 * The Service Protection Domain description including 
 *
 * - location information inside an address space, and a page table.
 * - size information regarding the static capabilities (max and
 *   current size)
 * - the identification permissions (which identification bytes this
 *   spd can control.)
 * - The address of the user-level static capabilities structure
 * - The static capabilities for this service.
 */

typedef enum {
	/* Is this spd a composite_spd? */
	SPD_COMPOSITE = 0x1, 
	/* currently unused */
	SPD_FREE = 0x2, 
	/* Must have SPD_COMPOSITE.  This spd should no longer be used
         * except for thread returns. */
	SPD_DEPRICATED = 0x4, 
	/* must be a spd_composite.  uses the page table of another
         * composite spd, so if we delete this, do not delete the
         * pgtbl, just decriment the reference count of the spd that
         * has the pg_tbl (via master_spd ptr) */
	SPD_SUBORDINATE = 0x8, 
	/* spd is active, can have capabilities assigned to it, and
	 * threads executed within it... */
	SPD_ACTIVE = 0x10, 
	/* Max number of descriptors for composite spds */
	MAX_MPD_DESC = 1024
} spd_flags_t;

/*
 * The spd_poly struct contains all the information that both struct
 * spds and struct composite_spds contains.  In essence, we want them
 * to be polymorphic with spd_poly as the "base class".
 */
struct spd_poly {
	spd_flags_t flags;
#if NUM_CPU_COS > 1
	atomic_t ref_cnt CACHE_ALIGNED;
#else
	atomic_t ref_cnt;
#endif
	paddr_t pg_tbl;
};

/* 
 * A collection of spds in the same protection domain.
 *
 * Spd membership in this composite can be tested by seeing if a
 * specific spd's address range is present in the spd_info->pg_tbl
 */
struct spd;
struct composite_spd {
	struct spd_poly spd_info;
	struct spd *members;               /* iff !(flags & (SPD_DEPRICATED|SPD_SUBORDINATE)) */
	struct composite_spd *master_spd;  /* iff flags & SPD_SUBORDINATE */
	struct composite_spd *next, *prev; /* flags & SPD_FREE -> next == freelist |
					    * flags & SPD_SUBORDINATE -> list of subordinates */
} CACHE_ALIGNED;

/* 
 * Currently spds consist of a contiguous range of virtual
 * addresses. if lowest_addr == 0, then we share page tables with the
 * main configuration task, thus having universal memory access.
 * Please do _not_ make the assumption that this (lowest_addr == 0 is
 * the linux task) in your code.  It will change in the near future.
 */
struct spd_location {
	unsigned int lowest_addr;
	unsigned int size;
};

typedef int mmaps_t;

struct spd {
	/* data touched on the ipc hotpath (32 bytes)*/
	struct spd_poly spd_info;
	union fork_counter fork;
	struct spd_location location[MAX_SPD_VAS_LOCATIONS];
	/* 
	 * The "current" protection state of the spd, which might
	 * point directly to spd->spd_info, or a
	 * composite_spd->spd_info
	 */
	struct spd_poly *composite_spd; 
	
	/* numbered faults correspond to which capability? */
	unsigned short int fault_handler[COS_FLT_MAX];
	/*
	 * user_cap_tbl is a pointer to the virtual address within the
	 * kernel address space of the user level capability table,
	 * while user_vaddr_cap_tbl is a pointer into actual
	 * component-space.
	 */
	struct usr_inv_cap *user_cap_tbl, *user_vaddr_cap_tbl;

	/* if this service is a scheduler, at what depth is it, and
	 * who's its parent? sched_depth < 0 if not schedulert */
	int sched_depth;
	struct spd *parent_sched;

	struct cos_sched_data_area *sched_shared_page[NUM_CPU], *kern_sched_shared_page[NUM_CPU];
	unsigned short int prev_notification[NUM_CPU];
	struct cos_net_xmit_headers *cos_net_xmit_headers[NUM_CPU];

	mmaps_t local_mmaps; /* mm_handle (see hijack.c) for linux compat */

	vaddr_t upcall_entry;
	vaddr_t async_inv_entry;
	
	vaddr_t atomic_sections[COS_NUM_ATOMIC_SECTIONS];
	
	unsigned int pfn_base, pfn_extent;

	/* should be a union to not waste space */
	struct spd *freelist_next;
	/* Linked list of the members of a non-depricated, current composite spd */
	struct spd *composite_member_next, *composite_member_prev;

	unsigned int ncaps;
	struct invocation_cap caps[MAX_STATIC_CAP];
	struct async_cap acaps[MAX_NUM_ACAP];
} CACHE_ALIGNED; //cache line size

paddr_t spd_alloc_pgtbl(void);
void spd_free_pgtbl(paddr_t pa);
struct spd *spd_alloc(unsigned short int max_static_cap, struct usr_inv_cap *usr_cap_tbl, 
		      vaddr_t upcall_entry);
int spd_set_location(struct spd *spd, unsigned long lowest_addr, 
		     unsigned long size, paddr_t pg_tbl);
int spd_add_location(struct spd *spd, long base, long size);
int spd_rem_location(struct spd *spd, long base, long size);
void spd_free(struct spd *spd);

int spd_is_free(int idx);
extern struct spd spds[MAX_NUM_SPDS];
static inline int spd_get_index(struct spd *spd)
{
//	return ((unsigned long)spd-(unsigned long)spds)/sizeof(struct spd);
	return (int)(spd-&spds[0]);
}
struct spd *spd_get_by_index(int idx);
void spd_free_all(void);
void spd_init(void);

int spd_set_fork_cnt(struct spd *spd, int n);
int spd_get_fork_cnt(struct spd *spd);
int spd_set_fork_origin(struct spd *spd, int origin);
int spd_get_fork_origin(struct spd *spd);

int spd_cap_activate(struct spd *spd, int cap);
int spd_cap_set_dest(struct spd *spd, int cap, struct spd* dspd);
int spd_cap_set_cstub(struct spd *spd, int cap, vaddr_t fn);
int spd_cap_set_sstub(struct spd *spd, int cap, vaddr_t fn);
int spd_cap_set_sfn(struct spd *spd, int cap, vaddr_t fn);
int spd_cap_set_fork_cnt(struct spd *spd, int cap, u8_t send, u8_t receive);
int spd_cap_get_fork_cnt(struct spd *spd, int cap);
int spd_cap_inc_fork_cnt(struct spd *spd, int cap, u8_t send, u8_t receive);
int spd_cap_set_fault_handler(struct spd *spd, int cap, int handler_num);

unsigned int spd_add_static_cap(struct spd *spd, vaddr_t service_entry_inst, struct spd *trusted_spd, 
				isolation_level_t isolation_level);
unsigned int spd_add_static_cap_extended(struct spd *spd, struct spd *trusted_spd, 
					 int cap_offset, vaddr_t ST_entry_fn,
					 vaddr_t AT_cli_stub, vaddr_t AT_serv_stub,
					 vaddr_t SD_cli_stub, vaddr_t SD_serv_stub,
					 isolation_level_t isolation_level, int flags);
isolation_level_t cap_change_isolation(struct spd *spd, int cap_num, isolation_level_t il, int flags);
int cap_is_free(struct spd *spd, int cap_num);
unsigned long spd_read_reset_invocation_cnt(struct spd *cspd, struct spd *sspd);

static inline int spd_is_scheduler(struct spd *spd)
{
	return spd->sched_depth >= 0;
}
static inline int spd_is_root_sched(struct spd *spd)
{
	return spd->sched_depth == 0;
}
static inline int spd_is_member(struct spd *spd, struct composite_spd *cspd)
{ 
	return spd->composite_spd == &cspd->spd_info;
}
static inline int spd_is_composite(struct spd_poly *info)
{ 
	return info->flags & SPD_COMPOSITE;
}
static inline int spd_is_active(struct spd *s)
{
	return s->spd_info.flags | SPD_ACTIVE;
}
static inline void spd_make_active(struct spd *s)
{
	s->spd_info.flags |= SPD_ACTIVE;
}
static inline int spd_mpd_is_depricated(struct composite_spd *mpd)
{ 
	return (mpd)->spd_info.flags & SPD_DEPRICATED;
}
static inline int spd_mpd_is_subordinate(struct composite_spd *mpd)
{ 
	return (mpd)->spd_info.flags & SPD_SUBORDINATE;
}

static inline void spd_mpd_reset_flags(struct composite_spd *mpd)
{
	mpd->spd_info.flags = 0;
}

static inline void spd_mpd_set_flags(struct composite_spd *mpd, int flags)
{
	mpd->spd_info.flags |= flags;
}

static inline void spd_mpd_remove_flags(struct composite_spd *mpd, int flags)
{
	mpd->spd_info.flags &= ~flags;
}

void spd_init_mpd_descriptors(void);
short int spd_alloc_mpd_desc(void);

void spd_mpd_release_desc(short int desc);
void spd_mpd_release(struct composite_spd *cspd);
static inline void spd_mpd_take(struct composite_spd *cspd)
{
	cos_ref_take(&cspd->spd_info.ref_cnt);
#ifdef __KERNEL__
//	printk("cos: take %p (%d)\n", cspd, cos_ref_val(&cspd->spd_info.ref_cnt));
#endif
}

struct composite_spd *spd_mpd_by_idx(short int idx);
short int spd_mpd_index(struct composite_spd *cspd);
static inline void spd_mpd_depricate(struct composite_spd *mpd)
{
	//printk("cos: depricating cspd %d.\n", spd_mpd_index(mpd));
	spd_mpd_set_flags(mpd, SPD_DEPRICATED);
	spd_mpd_release(mpd);
}
void spd_mpd_make_subordinate(struct composite_spd *master, struct composite_spd *slave);
static inline struct composite_spd *spd_alloc_mpd(void)
{
	return spd_mpd_by_idx(spd_alloc_mpd_desc());
}
static inline void spd_mpd_ipc_release(struct composite_spd *cspd)
{
	cos_meas_event(COS_MPD_IPC_REFCNT_DEC); 
	spd_mpd_release(cspd);
}
static inline void spd_mpd_ipc_take(struct composite_spd *cspd)
{
	cos_meas_event(COS_MPD_IPC_REFCNT_INC); 
	spd_mpd_take(cspd);
}

int spd_composite_add_member(struct composite_spd *cspd, struct spd *spd);
int spd_composite_remove_member(struct spd *spd, int remove_mappings);

/* FIXME: keep a count in the cspd to avoid this iteration */
static inline int spd_composite_num_members(struct composite_spd *cspd) 
{
	int num_members = 0;
	struct spd *iter;

	iter = cspd->members;
	if (iter) do {
		num_members++;
		iter = iter->composite_member_next;
		//assert(iter);
	} while (iter != cspd->members);

	return num_members;
}

/*
 * Move spd from its current composite spd to the cspd_new
 * composite_spd.
 */
static inline int spd_composite_move_member(struct composite_spd *cspd_new, struct spd *spd, int remove_mappings)
{
	/* 
	 * FIXME: in a multi-processor context, these should be
	 * probably be done in the opposite order.
	 */
	if (spd_composite_remove_member(spd, remove_mappings) ||
	    spd_composite_add_member(cspd_new, spd)) {
		return -1;
	}

	return 0;
}

struct spd *virtual_namespace_query(unsigned long addr);
int virtual_namespace_alloc(struct spd *spd, unsigned long addr, unsigned int size);

/*
 * FIXME: this should take a range of addresses, but since each
 * component < 4MB here, that is unneeded.
 *
 * FIXME: TEST THIS!
 */
static inline int spd_composite_member(struct spd *spd, struct spd_poly *poly)
{
	vaddr_t lowest_addr = spd->location[0].lowest_addr;

	return !chal_pgtbl_entry_absent(poly->pg_tbl, lowest_addr);
}

#else /* ASM */

/* WRONG */
#define SPD_CAP_TBL_PTR 8
#define SPD_CAP_TBL_SZ 12
#define SPD_LOWER_ADDR 0
#define SPD_REGION_SIZE 4


#define CAP_SIZE 8
#define CAP_ENTRY_INST 0
#define CAP_SPD 4

#endif

#endif /* SPD_H */
