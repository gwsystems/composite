/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>

/* dependencies */
#include <print.h>
#include <mem_mgr.h>
#include <sched.h>
#include <cos_alloc.h>
#include <cgraph.h>

/* interfaces */
#include <cinfo.h>
#include <failure_notif.h>

#include <cobj_format.h>
#include <cos_vect.h>

#define LOCK() if(sched_component_take(cos_spd_id())) BUG();
#define UNLOCK() if(sched_component_release(cos_spd_id())) BUG();

COS_VECT_CREATE_STATIC(spd_info_addresses);
extern struct cos_component_information cos_comp_info;
struct cobj_header *hs[MAX_NUM_SPDS+1];

/* local meta-data to track the components */
struct spd_local_md {
	spdid_t spdid;
	vaddr_t comp_info;
	char *page_start, *page_end;
	struct cobj_header *h;
} local_md[MAX_NUM_SPDS+1];

int cinfo_map(spdid_t spdid, vaddr_t map_addr, spdid_t target)
{
	vaddr_t cinfo_addr;

	cinfo_addr = (vaddr_t)cos_vect_lookup(&spd_info_addresses, target);
	if (0 == cinfo_addr) return -1;
	if (map_addr != 
	    (mman_alias_page(cos_spd_id(), cinfo_addr, spdid, map_addr))) {
		return -1;
	}

	return 0;
}

spdid_t cinfo_get_spdid(int iter)
{
	if (iter > MAX_NUM_SPDS) return 0;
	if (hs[iter] == NULL) return 0;

	return hs[iter]->id;
}

static int boot_spd_set_symbs(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci)
{
	int i;

	if (cos_spd_cntl(COS_SPD_UCAP_TBL, spdid, ci->cos_user_caps, 0)) BUG();
	if (cos_spd_cntl(COS_SPD_UPCALL_ADDR, spdid, ci->cos_upcall_entry, 0)) BUG();
	for (i = 0 ; i < COS_NUM_ATOMIC_SECTIONS/2 ; i++) {
		if (cos_spd_cntl(COS_SPD_ATOMIC_SECT, spdid, ci->cos_ras[i].start, i*2)) BUG();
		if (cos_spd_cntl(COS_SPD_ATOMIC_SECT, spdid, ci->cos_ras[i].end,   (i*2)+1)) BUG();
	}

	return 0;
}

static int boot_spd_symbs(struct cobj_header *h, spdid_t spdid, vaddr_t *comp_info)
{
	unsigned int i;

	for (i = 0 ; i < h->nsymb ; i++) {
		struct cobj_symb *symb;

		symb = cobj_symb_get(h, i);
		assert(symb);
		if (COBJ_SYMB_UNDEF == symb->type) break;

		switch (symb->type) {
		case COBJ_SYMB_COMP_INFO:
			*comp_info = symb->vaddr;
			break;
		default:
			printc("boot: Unknown symbol type %d\n", symb->type);
			break;
		}
	}
	return 0;
}

static void boot_symb_reify(char *mem, vaddr_t d_addr, vaddr_t symb_addr, u32_t value)
{
	if (round_to_page(symb_addr) == d_addr) {
		u32_t *p;
		
		p = (u32_t*)(mem + ((PAGE_SIZE-1) & symb_addr));
		*p = value;
	}
}

static void boot_symb_reify_16(char *mem, vaddr_t d_addr, vaddr_t symb_addr, u16_t value)
{
	if (round_to_page(symb_addr) == d_addr) {
		u16_t *p;
		
		p = (u16_t*)(mem + ((PAGE_SIZE-1) & symb_addr));
		*p = value;
	}
}

static void boot_symb_process(struct cobj_header *h, spdid_t spdid, vaddr_t heap_val, char *mem, 
			      vaddr_t d_addr, vaddr_t symb_addr)
{
	if (round_to_page(symb_addr) == d_addr) {
		struct cos_component_information *ci;
		
		ci = (struct cos_component_information*)(mem + ((PAGE_SIZE-1) & symb_addr));
//		ci->cos_heap_alloc_extent = ci->cos_heap_ptr;
//		ci->cos_heap_allocated = heap_val;
		if (!ci->cos_heap_ptr) ci->cos_heap_ptr = heap_val;
		ci->cos_this_spd_id = spdid;

		/* save the address of this page for later retrieval
		 * (e.g. to manipulate the stack pointer) */
		if (!cos_vect_lookup(&spd_info_addresses, spdid)) {
			boot_spd_set_symbs(h, spdid, ci);
			cos_vect_add_id(&spd_info_addresses, (void*)round_to_page(ci), spdid);
		}
	}
}

static vaddr_t boot_spd_end(struct cobj_header *h)
{
	struct cobj_sect *sect;
	int max_sect;

	max_sect = h->nsect-1;
	sect = cobj_sect_get(h, max_sect);
	
	return sect->vaddr + round_up_to_page(sect->bytes);
}

static int boot_spd_map_memory(struct cobj_header *h, spdid_t spdid, vaddr_t comp_info)
{
	unsigned int i;
	vaddr_t dest_daddr;

	local_md[spdid].spdid = spdid;
	local_md[spdid].h = h;
	local_md[spdid].page_start = cos_get_heap_ptr();
	local_md[spdid].comp_info = comp_info;
	for (i = 0 ; i < h->nsect ; i++) {
		struct cobj_sect *sect;
		char *dsrc;
		int left;

		sect = cobj_sect_get(h, i);
		dest_daddr = sect->vaddr;
		left = cobj_sect_size(h, i);

		while (left > 0) {
			dsrc = cos_get_vas_page();
			if ((vaddr_t)dsrc != mman_get_page(cos_spd_id(), (vaddr_t)dsrc, 0)) BUG();
			if (dest_daddr != (mman_alias_page(cos_spd_id(), (vaddr_t)dsrc, spdid, dest_daddr))) BUG();

			dest_daddr += PAGE_SIZE;
			left -= PAGE_SIZE;
		}
	}
	local_md[spdid].page_end = (void*)dest_daddr;

	return 0;
}

static int boot_spd_map_populate(struct cobj_header *h, spdid_t spdid, vaddr_t comp_info)
{
	unsigned int i;
	char *start_page;
	
	start_page = local_md[spdid].page_start;
	for (i = 0 ; i < h->nsect ; i++) {
		struct cobj_sect *sect;
		vaddr_t dest_daddr;
		char *lsrc, *dsrc;
		int left, page_left;

		sect = cobj_sect_get(h, i);
		dest_daddr = sect->vaddr;
		lsrc = cobj_sect_contents(h, i);
		left = cobj_sect_size(h, i);

		while (left) {
			/* data left on a page to copy over */
			page_left = (left > PAGE_SIZE) ? PAGE_SIZE : left;
			dsrc = start_page;
			start_page += PAGE_SIZE;

			if (sect->flags & COBJ_SECT_ZEROS) {
				memset(dsrc, 0, PAGE_SIZE);
			} else {
				memcpy(dsrc, lsrc, page_left);
				if (page_left < PAGE_SIZE) memset(dsrc+page_left, 0, PAGE_SIZE - page_left);
			}

			/* Check if special symbols that need
			 * modification are in this page */
			boot_symb_process(h, spdid, boot_spd_end(h), dsrc, dest_daddr, comp_info);
			
			lsrc += PAGE_SIZE;
			dest_daddr += PAGE_SIZE;
			left -= page_left;
		}
	}
	return 0;
}

static int boot_spd_map(struct cobj_header *h, spdid_t spdid, vaddr_t comp_info)
{
	if (boot_spd_map_memory(h, spdid, comp_info) || 
	    boot_spd_map_populate(h, spdid, comp_info)) return -1;

	return 0;
}

static int boot_spd_reserve_caps(struct cobj_header *h, spdid_t spdid)
{
	if (cos_spd_cntl(COS_SPD_RESERVE_CAPS, spdid, h->ncap, 0)) BUG();
	return 0;
}

/* Deactivate or activate all capabilities to an spd (i.e. call faults
 * on invocation, or use normal stubs) */
static int boot_spd_caps_chg_activation(spdid_t spdid, int activate)
{
	int i;

	/* Find all capabilities to spdid */
	for (i = 0 ; hs[i] != NULL ; i++) {
		unsigned int j;

		if (hs[i]->id == spdid) continue;
		for (j = 0 ; j < hs[i]->ncap ; j++) {
			struct cobj_cap *cap;

			cap = cobj_cap_get(hs[i], j);
			if (cobj_cap_undef(cap)) break;

			if (cap->dest_id != spdid) continue;

			cos_cap_cntl(COS_CAP_SET_SSTUB, hs[i]->id, cap->cap_off, activate ? cap->sstub : 1);
		}
	}
	return 0;
}

static void
boot_edge_create(spdid_t src, spdid_t dest)
{
	int i , c, s;

	for (i = 0 ; (c = cgraph_client(i)) >= 0 ; i++) {
		s = cgraph_server(i);
		if (c == src && s == dest) return;
	}
	cgraph_add(dest, src);
}

/* 
 * This must happen _after_ the memory is mapped in so that the
 * ucap_tbl can be found in the spd
 */
static int boot_spd_caps(struct cobj_header *h, spdid_t spdid)
{
	struct cobj_cap *cap;
	unsigned int i;

	for (i = 0 ; i < h->ncap ; i++) {
		cap = cobj_cap_get(h, i);

		if (cobj_cap_undef(cap)) break;

		/* we have a fault handler... */
		if (cap->fault_num < COS_NUM_FAULTS) {
			if (cos_cap_cntl(COS_CAP_SET_FAULT, spdid, cap->cap_off, cap->fault_num)) BUG();
		}
		
		if (cos_cap_cntl(COS_CAP_SET_SERV_FN, spdid, cap->cap_off, cap->sfn) ||
		    cos_cap_cntl(COS_CAP_SET_CSTUB, spdid, cap->cap_off, cap->cstub) ||
		    cos_cap_cntl(COS_CAP_SET_SSTUB, spdid, cap->cap_off, cap->sstub) ||
		    cos_cap_cntl(COS_CAP_ACTIVATE, spdid, cap->cap_off, cap->dest_id)) BUG();
		
		boot_edge_create(spdid, cap->dest_id);
	}

	return 0;
}

static int boot_spd_thd(spdid_t spdid)
{
	int new_thd;

	/* Create a thread IF the component requested one */
	if ((new_thd = sched_create_thread_default(cos_spd_id(), spdid)) < 0) return -1;
	return new_thd;
}

static void boot_find_cobjs(struct cobj_header *h, int n)
{
	int i;
	vaddr_t start, end;

	start = (vaddr_t)h;
	hs[0] = h;
	for (i = 1 ; i < n ; i++) {
		int j = 0, size = 0, tot = 0;

		size = h->size;
		for (j = 0 ; j < (int)h->nsect ; j++) {
			tot += cobj_sect_size(h, j);
		}
		printc("cobj %d found at %p:%d, size %d -> %x\n", 
		       i, hs[i-1], size, tot, cobj_sect_get(hs[i-1], 0)->vaddr);

		end = start + round_up_to_cacheline(size);
		hs[i] = h = (struct cobj_header*)end;
		start = end;
	}
	hs[n] = NULL;
	printc("cobj found at %p ... -> %x\n", 
	       hs[n-1], cobj_sect_get(hs[n-1], 0)->vaddr);
}

static void boot_create_system(void)
{
	int i;
	
	for (i = 0 ; hs[i] != NULL ; i++) {
		struct cobj_header *h;
		spdid_t spdid;
		struct cobj_sect *sect;
		vaddr_t comp_info = 0;
		
		h = hs[i];
		if ((spdid = cos_spd_cntl(COS_SPD_CREATE, 0, 0, 0)) == 0) BUG();
		//printc("spdid %d, h->id %d\n", spdid, h->id);
		assert(spdid == h->id);
		sect = cobj_sect_get(h, 0);
		if (cos_spd_cntl(COS_SPD_LOCATION, spdid, sect->vaddr, SERVICE_SIZE)) BUG();
		
		if (boot_spd_symbs(h, spdid, &comp_info)) BUG();
		if (boot_spd_map(h, spdid, comp_info)) BUG();
		if (boot_spd_reserve_caps(h, spdid)) BUG();
		if (cos_spd_cntl(COS_SPD_ACTIVATE, spdid, 0, 0)) BUG();
	}
	for (i = 0 ; hs[i] != NULL ; i++) {
		struct cobj_header *h;
		h = hs[i];

		if (boot_spd_caps(h, h->id)) BUG();
	}
	for (i = 0 ; hs[i] != NULL ; i++) {
		struct cobj_header *h;
		h = hs[i];
		
		boot_spd_thd(h->id);
	}
}

void failure_notif_wait(spdid_t caller, spdid_t failed)
{
	/* wait for the other thread to finish rebooting the component */
	LOCK();	
	UNLOCK();
}

/* Reboot the failed component! */
void failure_notif_fail(spdid_t caller, spdid_t failed)
{
	struct spd_local_md *md;

	LOCK();

	boot_spd_caps_chg_activation(failed, 0);
	md = &local_md[failed];
	assert(md);
	if (boot_spd_map_populate(md->h, failed, md->comp_info)) BUG();
	boot_spd_thd(failed); 	/* can fail if component had no boot threads! */
	boot_spd_caps_chg_activation(failed, 1);

	UNLOCK();
}

#define INIT_STR_SZ 52

/* struct is 64 bytes, so we can have 64 entries in a page. */
struct component_init_str {
	unsigned int spdid, schedid;
	int startup;
	char init_str[INIT_STR_SZ];
}__attribute__((packed));
struct deps { int from, to; };

void cos_init(void *arg)
{
	struct cobj_header *h;
	int num_cobj;
	struct deps *ds;
	struct component_init_str *init_args;

	LOCK();
	cos_vect_init_static(&spd_info_addresses);
	h         = (struct cobj_header *)cos_comp_info.cos_poly[0];
	num_cobj  = (int)cos_comp_info.cos_poly[1];

	ds        = (struct deps *)cos_comp_info.cos_poly[2];
	init_args = (struct component_init_str *)cos_comp_info.cos_poly[3];
	init_args++; 

	boot_find_cobjs(h, num_cobj);
	/* This component really might need more vas */
	if (cos_vas_cntl(COS_VAS_SPD_EXPAND, cos_spd_id(), 
			 round_up_to_pgd_page((unsigned long)&num_cobj), 
			 round_up_to_pgd_page(1))) {
		printc("Could not expand boot component to %p:%x\n",
		       (void *)round_up_to_pgd_page((unsigned long)&num_cobj), 
		       (unsigned int)round_up_to_pgd_page(1));
		BUG();
	}

	printc("h @ %p, heap ptr @ %p\n", h, cos_get_heap_ptr());
	printc("header %p, size %d, num comps %d, new heap %p\n", 
	       h, h->size, num_cobj, cos_get_heap_ptr());

	/* Assumes that hs have been setup with boot_find_cobjs */
	boot_create_system();
	UNLOCK();

	return;
}
