/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
/* dependencies */
#include <boot_deps.h>

extern struct cos_component_information cos_comp_info;
struct cobj_header *hs[MAX_NUM_SPDS+1];


#include <cobj_format.h>

/* interfaces */
//#include <failure_notif.h>
#include <cgraph.h>

/* local meta-data to track the components */
struct spd_local_md {
	// old format
	spdid_t spdid;
	vaddr_t comp_info;
	char *page_start, *page_end;
	struct cobj_header *h;
} local_md[MAX_NUM_SPDS+1];

/* Component initialization info. */
#define INIT_STR_SZ 52
/* struct is 64 bytes, so we can have 64 entries in a page. */
struct component_init_str {
	unsigned int spdid, schedid;
	int startup;
	char init_str[INIT_STR_SZ];
}__attribute__((packed));
struct component_init_str *init_args;

static int
boot_spd_set_symbs(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci)
{
	int i;

	if (cos_spd_cntl(COS_SPD_UCAP_TBL, spdid, ci->cos_user_caps, 0)) BUG();
	if (cos_spd_cntl(COS_SPD_UPCALL_ADDR, spdid, ci->cos_upcall_entry, 0)) BUG();
	if (cos_spd_cntl(COS_SPD_ASYNC_INV_ADDR, spdid, ci->cos_async_inv_entry, 0)) BUG();

	for (i = 0 ; i < COS_NUM_ATOMIC_SECTIONS/2 ; i++) {
		if (cos_spd_cntl(COS_SPD_ATOMIC_SECT, spdid, ci->cos_ras[i].start, i*2)) BUG();
		if (cos_spd_cntl(COS_SPD_ATOMIC_SECT, spdid, ci->cos_ras[i].end,   (i*2)+1)) BUG();
	}

	return 0;
}

static int
boot_spd_symbs(struct cobj_header *h, spdid_t spdid, vaddr_t *comp_info)
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

static void
boot_symb_reify(char *mem, vaddr_t d_addr, vaddr_t symb_addr, u32_t value)
{
	if (round_to_page(symb_addr) == d_addr) {
		u32_t *p;

		p = (u32_t*)(mem + ((PAGE_SIZE-1) & symb_addr));
		*p = value;
	}
}

static void
boot_symb_reify_16(char *mem, vaddr_t d_addr, vaddr_t symb_addr, u16_t value)
{
	if (round_to_page(symb_addr) == d_addr) {
		u16_t *p;

		p = (u16_t*)(mem + ((PAGE_SIZE-1) & symb_addr));
		*p = value;
	}
}

static int
boot_process_cinfo(struct cobj_header *h, spdid_t spdid, vaddr_t heap_val,
		   char *mem, vaddr_t symb_addr)
{
	int i;
	struct cos_component_information *ci;

	//if (round_to_page(symb_addr) != d_addr) return 0;

	assert(symb_addr == round_to_page(symb_addr));
	ci = (struct cos_component_information*)(mem);

	if (!ci->cos_heap_ptr) ci->cos_heap_ptr = heap_val;
	ci->cos_this_spd_id = spdid;
	ci->init_string[0]  = '\0';
	for (i = 0 ; init_args[i].spdid ; i++) {
		char *start, *end;
		int len;

		if (init_args[i].spdid != spdid) continue;

		start = strchr(init_args[i].init_str, '\'');
		if (!start) break;
		start++;
		end   = strchr(start, '\'');
		if (!end) break;
		len   = (int)(end-start);
		memcpy(&ci->init_string[0], start, len);
		ci->init_string[len] = '\0';
	}

	return 1;
}

static vaddr_t
boot_spd_end(struct cobj_header *h)
{
	struct cobj_sect *sect;
	int max_sect;

	max_sect = h->nsect-1;
	sect     = cobj_sect_get(h, max_sect);

	return sect->vaddr + round_up_to_page(sect->bytes);
}

static int
boot_spd_alloc_memory(struct cobj_header *h, spdid_t spdid, vaddr_t comp_info)
{
	unsigned int i;
	vaddr_t dest_daddr, prev_map = 0;
	char *dsrc;
	int flag;

	local_md[spdid].spdid      = spdid;
	local_md[spdid].h          = h;
	local_md[spdid].page_start = cos_get_heap_ptr();
	local_md[spdid].comp_info  = comp_info;

	/* save the address of the heap for later retrieval */
	boot_deps_save_hp(spdid, (void*)boot_spd_end(h));

	for (i = 0 ; i < h->nsect ; i++) {
		struct cobj_sect *sect = cobj_sect_get(h, i);
		int left = cobj_sect_size(h, i);
		sect = cobj_sect_get(h, i);
		dest_daddr = sect->vaddr;
		if (round_to_page(dest_daddr) == prev_map) {
			left -= (prev_map + PAGE_SIZE - dest_daddr);
			dest_daddr = prev_map + PAGE_SIZE;
		}

		while (left > 0) {
			dsrc = cos_get_vas_page();
			if ( i == 0 && left == (int)cobj_sect_size(h, 0) )
				assert(dsrc == local_md[spdid].page_start);
			left -= PAGE_SIZE;
			prev_map = dest_daddr;
			dest_daddr += PAGE_SIZE;
		}
	}
	local_md[spdid].page_end = dsrc + PAGE_SIZE;
	return 0;
}

static int
boot_spd_map_memory(struct cobj_header *h, spdid_t spdid, vaddr_t comp_info)
{
	unsigned int i, use_kmem;
	vaddr_t dest_daddr, prev_map = 0;
	char *dsrc;


	dsrc = local_md[spdid].page_start;

	for (i = 0 ; i < h->nsect ; i++) {
		struct cobj_sect *sect;
		int left;

		use_kmem   = 0;
		sect       = cobj_sect_get(h, i);
		if (sect->flags & COBJ_SECT_KMEM) use_kmem = 1;
		dest_daddr = sect->vaddr;
		left       = cobj_sect_size(h, i);
		/* previous section overlaps with this one, don't remap! */
		if (round_to_page(dest_daddr) == prev_map) {
			left -= (prev_map + PAGE_SIZE - dest_daddr);
			dest_daddr = prev_map + PAGE_SIZE;
		}
		if (left > 0) {
			left = round_up_to_page(left);
			prev_map = dest_daddr;
			boot_deps_map_sect(spdid, dsrc, dest_daddr, left/PAGE_SIZE, h, i);
			prev_map += left - PAGE_SIZE;
			dest_daddr += left;
			dsrc += left;
		}
	}
	assert(local_md[spdid].page_end == dsrc);

	return 0;
}

static int
boot_spd_map_populate(struct cobj_header *h, spdid_t spdid, vaddr_t comp_info, int first_time)
{
	unsigned int i;
	/* Where are we in the actual component's memory in the booter? */
	char *start_addr, *offset;
	/* Where are we in the destination address space? */
	vaddr_t prev_daddr, init_daddr;

	start_addr = local_md[spdid].page_start;
	init_daddr = cobj_sect_get(h, 0)->vaddr;

	for (i = 0 ; i < h->nsect ; i++) {
		struct cobj_sect *sect;
		vaddr_t dest_daddr;
		char *lsrc, *dsrc;
		int left, dest_doff;

		sect       = cobj_sect_get(h, i);
		/* virtual address in the destination address space */
		dest_daddr = sect->vaddr;
		/* where we're copying from in the cobj */
		lsrc       = cobj_sect_contents(h, i);
		/* how much is left to copy? */
		left       = cobj_sect_size(h, i);

		/* Initialize memory. */
		if (first_time || !(sect->flags & COBJ_SECT_INITONCE)) {
			if (sect->flags & COBJ_SECT_ZEROS) {
				memset(start_addr + (dest_daddr - init_daddr), 0, left);
			} else {
				memcpy(start_addr + (dest_daddr - init_daddr), lsrc, left);
			}
		}

		if (sect->flags & COBJ_SECT_CINFO) {
			struct cos_component_information *ci;
			assert(left == PAGE_SIZE);
			assert(comp_info == dest_daddr);
			ci = (struct cos_component_information *)(start_addr + (comp_info - init_daddr));
			boot_process_cinfo(h, spdid, boot_spd_end(h), (void*)ci, comp_info);
			if (first_time) boot_spd_set_symbs(h, spdid, ci);
		}
	}

 	return 0;
}

static int
boot_spd_map(struct cobj_header *h, spdid_t spdid, vaddr_t comp_info)
{
	if (boot_spd_map_memory(h, spdid, comp_info)) return -1;
	if (boot_spd_map_populate(h, spdid, comp_info, 1)) return -1;

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
		if (cap->fault_num < COS_FLT_MAX) {
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

/* The order of creating boot threads */
unsigned int *boot_sched;

static int
boot_spd_thd(spdid_t spdid)
{
	union sched_param sp = {.c = {.type = SCHEDP_RPRIO, .value = 1}};
	union sched_param sp_coreid;

	/* All init threads on core 0. */
	sp_coreid.c.type = SCHEDP_CORE_ID;
	sp_coreid.c.value = 0;

	/* Create a thread IF the component requested one */
	if ((sched_create_thread_default(spdid, sp.v, sp_coreid.v, 0)) < 0) return -1;
	return 0;
}

static void
boot_find_cobjs(struct cobj_header *h, int n)
{
	int i;
	vaddr_t start, end;

	start = (vaddr_t)h;
	hs[0] = h;
	for (i = 1 ; i < n ; i++) {
		int j = 0, size = 0, tot = 0;

		size = h->size;
		for (j = 0 ; j < (int)h->nsect ; j++) {
			//printc("\tsection %d, size %d\n", j, cobj_sect_size(h, j));
			tot += cobj_sect_size(h, j);
		}
		printc("cobj %s:%d found at %p:%x, size %x -> %x\n",
		       h->name, h->id, hs[i-1], size, tot, cobj_sect_get(hs[i-1], 0)->vaddr);

		end   = start + round_up_to_cacheline(size);
		hs[i] = h = (struct cobj_header*)end;
		start = end;
	}

	hs[n] = NULL;
	printc("cobj %s:%d found at %p -> %x\n",
	       hs[n-1]->name, hs[n-1]->id, hs[n-1], cobj_sect_get(hs[n-1], 0)->vaddr);
}

#define NREGIONS 4

static void
boot_create_system(void)
{
	unsigned int i;

	for (i = 0 ; hs[i] != NULL ; i++) {
		struct cobj_header *h;
		spdid_t spdid;
		struct cobj_sect *sect;
		vaddr_t comp_info = 0;
		long tot = 0;
		int j;

		h = hs[i];
		if ((spdid = cos_spd_cntl(COS_SPD_CREATE, 0, 0, 0)) == 0) BUG();
		/* printc("spdid %d, h->id %d\n", spdid, h->id); */
		assert(spdid == h->id);

		sect = cobj_sect_get(h, 0);
		if (cos_spd_cntl(COS_SPD_LOCATION, spdid, sect->vaddr, SERVICE_SIZE)) BUG();
		for (j = 0 ; j < (int)h->nsect ; j++) {
			tot += cobj_sect_size(h, j);
		}

		if (tot > SERVICE_SIZE) {
			if (cos_vas_cntl(COS_VAS_SPD_EXPAND, h->id, sect->vaddr + SERVICE_SIZE,
					 (NREGIONS-1) * round_up_to_pgd_page(1))) {
				printc("cos: booter could not expand VAS for component %d\n", h->id);
				BUG();
			}
			if (hs[i + 1] != NULL && cobj_sect_get(hs[i + 1], 0)->vaddr != sect->vaddr + SERVICE_SIZE * 4) {
				/* We only need to expand to the next 4MB
				 * region for now. The start address of the
				 * next component should have no overlap with
				 * the current one. */
				BUG();
			}
		}
		if (boot_spd_symbs(h, spdid, &comp_info))        BUG();
		if (boot_spd_alloc_memory(h, spdid, comp_info)) BUG();
	}

	boot_deps_save_hp(cos_spd_id(), cos_get_heap_ptr());
	printc("booter: heap pointer now at %p\n", cos_get_heap_ptr());

	for (i = 0 ; hs[i] != NULL ; i++) {
		struct cobj_header *h = hs[i];
		vaddr_t comp_info = local_md[h->id].comp_info;
		if (boot_spd_map(h, h->id, comp_info))           BUG();
		if (cos_spd_cntl(COS_SPD_ACTIVATE, h->id, h->ncap, 0)) BUG();
	}

	for (i = 0 ; hs[i] != NULL ; i++) {
		struct cobj_header *h;
		h = hs[i];

		if (boot_spd_caps(h, h->id)) BUG();
	}

	if (!boot_sched) return;

	for (i = 0 ; boot_sched[i] != 0 ; i++) {
		struct cobj_header *h;
		int j;
		h = NULL;
		for (j = 0 ; hs[j] != NULL; j++) {
			if (hs[j]->id == boot_sched[i]) h = hs[j];
		}
		assert(h);
		if (h->flags & COBJ_INIT_THD) boot_spd_thd(h->id);
	}
}

void
failure_notif_wait(spdid_t caller, spdid_t failed)
{
	/* wait for the other thread to finish rebooting the component */
	LOCK();
	UNLOCK();
}

/* Reboot the failed component! */
void
failure_notif_fail(spdid_t caller, spdid_t failed)
{
	struct spd_local_md *md;

	LOCK();

//	boot_spd_caps_chg_activation(failed, 0);
	md = &local_md[failed];
	assert(md);
	if (boot_spd_map_populate(md->h, failed, md->comp_info, 0)) BUG();
	/* can fail if component had no boot threads: */
	if (md->h->flags & COBJ_INIT_THD) boot_spd_thd(failed);
	if (boot_spd_caps(md->h, failed)) BUG();
//	boot_spd_caps_chg_activation(failed, 1);

	UNLOCK();
}

#define MAX_DEP (PAGE_SIZE/sizeof(struct deps))
struct deps { short int client, server; };
struct deps deps[MAX_DEP];
int ndeps;

int
cgraph_server(int iter)
{
	if (iter >= ndeps || iter < 0) return -1;
	return deps[iter].server;
}

int
cgraph_client(int iter)
{
	if (iter >= ndeps || iter < 0) return -1;
	return deps[iter].client;
}

int
cgraph_add(int serv, int client)
{
	if (ndeps == MAX_DEP) return -1;
	deps[ndeps].server = serv;
	deps[ndeps].client = client;
	ndeps++;
	return 0;
}

/****************************************************/
/* Functions using new kernel cap operations below. */
/****************************************************/

static int
boot_comp_map_memory(struct cobj_header *h, spdid_t spdid, vaddr_t comp_info)
{
	unsigned int i;
	vaddr_t dest_daddr, prev_map = 0;
	char *dsrc;
	int flag;
	capid_t captbl_cap = comp_cap_info[spdid].captbl_cap[0];
	capid_t pgtbl_cap  = comp_cap_info[spdid].pgtbl_cap;

	/* We'll map the component into booter's heap. */
	comp_cap_info[spdid].vaddr_mapped_in_booter = (vaddr_t)cos_get_heap_ptr();

	for (i = 0 ; i < h->nsect ; i++) {
		struct cobj_sect *sect;
		int left;

		sect = cobj_sect_get(h, i);
		flag = MAPPING_RW;
		if (sect->flags & COBJ_SECT_KMEM) {
			flag |= MAPPING_KMEM;
		}

		dest_daddr = sect->vaddr;
		left       = cobj_sect_size(h, i);
		/* previous section overlaps with this one, don't remap! */
		if (round_to_page(dest_daddr) == prev_map) {
			left -= (prev_map + PAGE_SIZE - dest_daddr);
			dest_daddr = prev_map + PAGE_SIZE;
		}
		while (left > 0) {
			//FIXME: use kmem if (flag & MAPPING_KMEM)
			vaddr_t addr = get_pmem();

			if (!addr) BUG();
			if (call_cap_op(BOOT_CAPTBL_SELF_PT, CAPTBL_OP_CPY,
					addr, pgtbl_cap, dest_daddr, 0)) BUG();
			prev_map = dest_daddr;
			dest_daddr += PAGE_SIZE;
			left       -= PAGE_SIZE;
		}
	}

	return 0;
}

static int
boot_comp_map_populate(struct cobj_header *h, spdid_t spdid, vaddr_t comp_info, int first_time)
{
	unsigned int i;
	/* Where are we in the actual component's memory in the booter? */
	char *start_addr, *offset;
	/* Where are we in the destination address space? */
	vaddr_t prev_daddr, init_daddr;
	struct cos_component_information *ci;

	start_addr = (char *)(comp_cap_info[spdid].vaddr_mapped_in_booter);
	init_daddr = cobj_sect_get(h, 0)->vaddr;

	for (i = 0 ; i < h->nsect ; i++) {
		struct cobj_sect *sect;
		vaddr_t dest_daddr;
		char *lsrc, *dsrc;
		int left, dest_doff;

		sect       = cobj_sect_get(h, i);
		/* virtual address in the destination address space */
		dest_daddr = sect->vaddr;
		/* where we're copying from in the cobj */
		lsrc       = cobj_sect_contents(h, i);
		/* how much is left to copy? */
		left       = cobj_sect_size(h, i);

		/* Initialize memory. */
		if (!(sect->flags & COBJ_SECT_KMEM) &&
		    (first_time || !(sect->flags & COBJ_SECT_INITONCE))) {
			if (sect->flags & COBJ_SECT_ZEROS) {
				memset(start_addr + (dest_daddr - init_daddr), 0, left);
			} else {
				memcpy(start_addr + (dest_daddr - init_daddr), lsrc, left);
			}
		}

		if (sect->flags & COBJ_SECT_CINFO) {
			assert(left == PAGE_SIZE);
			assert(comp_info == dest_daddr);
			boot_process_cinfo(h, spdid, boot_spd_end(h), start_addr + (comp_info-init_daddr), comp_info);
			ci = (struct cos_component_information*)(start_addr + (comp_info-init_daddr));
			comp_cap_info[h->id].upcall_entry = ci->cos_upcall_entry;
		}
	}

 	return 0;
}

static int
boot_comp_map(struct cobj_header *h, spdid_t spdid, vaddr_t comp_info)
{
	if (boot_comp_map_memory(h, spdid, comp_info)) return -1;
	if (boot_comp_map_populate(h, spdid, comp_info, 1)) return -1;

	return 0;
}

/*
 * This must happen _after_ the memory is mapped in so that the
 * ucap_tbl can be found in the spd
 */
static int boot_comp_caps(struct cobj_header *h, spdid_t comp_id)
{
	struct cobj_cap *cap;
	unsigned int i;
	/* FIXME: We need cap allocation and management for llboot
	 * loaded components. */
	u32_t sinv_cap = 4; //0 is default return cap

	for (i = 0 ; i < h->ncap ; i++) {
		cap = cobj_cap_get(h, i);

		if (cobj_cap_undef(cap)) break;

		/* printc("cap from comp %d to %d, cap %d  %x activate\n", */
		/*        comp_id, cap->dest_id, sinv_cap, cap->sstub); */
		if (call_cap_op(comp_cap_info[comp_id].captbl_cap[0], CAPTBL_OP_SINVACTIVATE,
				sinv_cap, comp_cap_info[cap->dest_id].comp_cap, cap->sstub, 0)) BUG();
		sinv_cap += captbl_idsize(CAP_SINV);
	}

	/* round to a new entry */
	comp_cap_info[comp_id].cap_frontier = round_up_to_pow2(sinv_cap, CAPMAX_ENTRY_SZ);

	return 0;
}

static void init_cosframes(void)
{
	int i, n_kmem_sets = COS_KERNEL_MEMORY / RETYPE_MEM_NPAGES + 1;
	vaddr_t kmem_cap = BOOT_MEM_KM_BASE;
	int ret;

	for (i = 0; i < n_kmem_sets; i++) {
		ret = call_cap_op(BOOT_CAPTBL_SELF_PT, CAPTBL_OP_MEM_RETYPE2KERN,
				  kmem_cap, 0, 0, 0);
		if (ret) {
			/* llboot used some kmem already. Those used
			 * kmem pages are not mapped as cos_frame in
			 * our PGTBL. So we won't be able to retype
			 * all kmem frames. */
			n_kern_memsets = i;

			break;
		}
		kmem_cap += (RETYPE_MEM_NPAGES*PAGE_SIZE);
	}


}

static void
boot_create_cap_system(void)
{
	int ret;
	unsigned int i, min = ~0;

	for (i = 0 ; hs[i] != NULL ; i++) {
		if (hs[i]->id < min) min = hs[i]->id;
	}

	for (i = 0 ; hs[i] != NULL ; i++) {
		struct cobj_header *h;
		spdid_t spdid;
		struct cobj_sect *sect;
		vaddr_t comp_info = 0;
		long tot = 0;
		int j, n_pte;
		/* cap tbl, pgtbl for new component */
		capid_t comp_cap;
		capid_t captbl_cap, pgtbl_cap;
		capid_t pte_cap;

		u64_t lid = get_liv_id();

		h = hs[i];
		spdid = h->id;

		sect = cobj_sect_get(h, 0);
		for (j = 0 ; j < (int)h->nsect ; j++) {
			tot += cobj_sect_size(h, j);
		}

		n_pte = 1;
		if (tot > SERVICE_SIZE) {
			/* printc("Component %d size %ld greater than default size!\n", spdid, tot); */
			n_pte = tot / SERVICE_SIZE;
			if (tot % SERVICE_SIZE) n_pte++;
		}

		/* create cap tbl, pgtbl for new component */
		comp_cap    = alloc_capid(CAP_COMP);
		for (j = 0; j < BOOT_CAPTBL_NPAGES; j++) {
			comp_cap_info[spdid].captbl_cap[j] = alloc_capid(CAP_CAPTBL);
		}

		pgtbl_cap   = alloc_capid(CAP_PGTBL);

		/* Captbl */
		captbl_cap = comp_cap_info[spdid].captbl_cap[0];
		if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_CAPTBLACTIVATE, captbl_cap,
				BOOT_CAPTBL_SELF_PT, get_kmem_cap(), 0)) BUG();

		for (j = 1; j < BOOT_CAPTBL_NPAGES; j++) {
			/* Another page for the captbl. */
			if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_CAPTBLACTIVATE, comp_cap_info[spdid].captbl_cap[j],
					BOOT_CAPTBL_SELF_PT, get_kmem_cap(), 1)) BUG();
			/* Captbl expand */
			if (call_cap_op(captbl_cap, CAPTBL_OP_CONS, comp_cap_info[spdid].captbl_cap[j],
					(PAGE_SIZE*j - PAGE_SIZE/2)/CAPTBL_LEAFSZ, 0, 0)) BUG();
		}

		/* PGD */
		if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_PGTBLACTIVATE,
				pgtbl_cap, BOOT_CAPTBL_SELF_PT, get_kmem_cap(), 0))  BUG();

		/* PTEs */
		for (j = 0; j < n_pte; j++) {
			pte_cap = alloc_capid(CAP_PGTBL);
			if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_PGTBLACTIVATE,
					pte_cap, BOOT_CAPTBL_SELF_PT, get_kmem_cap(), 1))    BUG();

			/* Construct pgtbl */
			if (call_cap_op(pgtbl_cap, CAPTBL_OP_CONS, pte_cap, sect->vaddr + j*SERVICE_SIZE, 0, 0)) BUG();
		}

		comp_cap_info[spdid].pgtbl_cap  = pgtbl_cap;
		comp_cap_info[spdid].comp_cap   = comp_cap;
		comp_cap_info[spdid].addr_start = sect->vaddr;

		if (boot_spd_symbs(h, spdid, &comp_info))   BUG();
		if (boot_comp_map(h, spdid, comp_info))     BUG();

		if (call_cap_op(BOOT_CAPTBL_SELF_CT, CAPTBL_OP_COMPACTIVATE,
				comp_cap, (captbl_cap<<16) | pgtbl_cap, lid, comp_cap_info[spdid].upcall_entry)) BUG();

		printc("Comp %d (%s) activated @ %x, size %ld!\n", h->id, h->name, sect->vaddr, tot);
	}

	for (i = 0 ; hs[i] != NULL ; i++) {
		struct cobj_header *h;
		h = hs[i];

		if (boot_comp_caps(h, h->id)) BUG();
	}

	boot_comp_deps_init();

	return;
}

void cos_init(void)
{
	struct cobj_header *h;
	int num_cobj, i;

	LOCK();

	h         = (struct cobj_header *)cos_comp_info.cos_poly[0];
	num_cobj  = (int)cos_comp_info.cos_poly[1];

	memcpy(deps, (struct deps *)cos_comp_info.cos_poly[2], PAGE_SIZE);
	for (i = 0 ; deps[i].server ; i++) ;
	ndeps     = i;

	init_args = (struct component_init_str *)cos_comp_info.cos_poly[3];
	init_args++;

	boot_sched = (unsigned int *)cos_comp_info.cos_poly[4];

	boot_find_cobjs(h, num_cobj);

	printc("h @ %p, heap ptr @ %p\n", h, cos_get_heap_ptr());
	printc("header %p, size %d, num comps %d, new heap %p\n",
	       h, h->size, num_cobj, cos_get_heap_ptr());

	init_cosframes();
	boot_create_cap_system();
	printc("booter: done creating system.\n");

	UNLOCK();

	boot_deps_run();

	return;
}
