#include <cos_component.h>

#include <print.h>
#include <mem_mgr.h>
#include <sched.h>
#include <cos_alloc.h>
#include <cgraph.h>

#include <cobj_format.h>
#include <cos_vect.h>

COS_VECT_CREATE_STATIC(spd_info_addresses);
extern struct cos_component_information cos_comp_info;
struct cobj_header *hs[MAX_NUM_SPDS+1];

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
		boot_spd_set_symbs(h, spdid, ci);
		ci->cos_heap_ptr = heap_val;
		ci->cos_this_spd_id = spdid;

		/* save the address of this page for later retrieval
		 * (e.g. to manipulate the stack pointer) */
		cos_vect_add_id(&spd_info_addresses, (void*)round_to_page(ci), spdid);
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

static int boot_spd_map(struct cobj_header *h, spdid_t spdid, vaddr_t comp_info)
{
	unsigned int i;

	for (i = 0 ; i < h->nsect ; i++) {
		struct cobj_sect *sect;
		vaddr_t dest_daddr;
		char *lsrc, *dsrc;
		int left, page_left;
		//unsigned short int flags;

		sect = cobj_sect_get(h, i);
		dest_daddr = sect->vaddr;
		lsrc = cobj_sect_contents(h, i);
		left = cobj_sect_size(h, i);

		while (left) {
			/* data left on a page to copy over */
			page_left = (left > PAGE_SIZE) ? PAGE_SIZE : left;
			dsrc = cos_get_heap_ptr();
			cos_set_heap_ptr((void*)(((unsigned long)cos_get_heap_ptr()) + PAGE_SIZE));
			if ((vaddr_t)dsrc != mman_get_page(cos_spd_id(), (vaddr_t)dsrc, 0)) BUG();

			if (sect->flags & COBJ_SECT_ZEROS) {
				memset(dsrc, 0, PAGE_SIZE);
			} else {
				memcpy(dsrc, lsrc, page_left);
				if (page_left < PAGE_SIZE) memset(dsrc+page_left, 0, PAGE_SIZE - page_left);
			}

			/* Check if special symbols that need
			 * modification are in this page */
			boot_symb_process(h, spdid, boot_spd_end(h), dsrc, dest_daddr, comp_info);
			
			if (dest_daddr != (mman_alias_page(cos_spd_id(), (vaddr_t)dsrc, spdid, dest_daddr))) BUG();

			lsrc += PAGE_SIZE;
			dest_daddr += PAGE_SIZE;
			left -= page_left;
		}
	}
	return 0;
}

static int boot_spd_reserve_caps(struct cobj_header *h, spdid_t spdid)
{
	if (cos_spd_cntl(COS_SPD_RESERVE_CAPS, spdid, h->ncap, 0)) BUG();
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
	if ((new_thd = sched_create_thread_default(cos_spd_id(), spdid)) < 0) {
		return -1;
	}
	
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
		printc("cobj found at %p:%d, size %d -> %x\n", 
		       hs[i-1], size, tot, cobj_sect_get(h, 0)->vaddr);

		end = start + round_up_to_cacheline(size);
		hs[i] = h = (struct cobj_header*)end;
		start = end;
	}
	hs[n] = NULL;
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
		//printc("loaded spdid %d \n", h->id);
	}
	for (i = 0 ; hs[i] != NULL ; i++) {
		struct cobj_header *h;
		h = hs[i];
		
		boot_spd_thd(h->id);
	}
}

void cos_init(void *arg)
{
	struct cobj_header *h;
	int num_cobj;

	cos_vect_init_static(&spd_info_addresses);
	h = (struct cobj_header *)cos_comp_info.cos_poly[0];
	num_cobj = (int)cos_comp_info.cos_poly[1];
	boot_find_cobjs(h, num_cobj);

	printc("h @ %p, heap ptr @ %p\n", h, cos_get_heap_ptr());
	printc("header %p, size %d, num comps %d, new heap %p\n", 
	       h, h->size, num_cobj, cos_get_heap_ptr());

	/* Assumes that hs have been setup with boot_find_cobjs */
	boot_create_system();

	return;
}

void bin (void)
{
	sched_block(cos_spd_id(), 0);
}

