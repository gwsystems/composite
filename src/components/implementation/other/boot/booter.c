#include <cos_component.h>
#include <print.h>

#include <mem_mgr.h>
#include <sched.h>
#include <cos_alloc.h>
#include <cobj_format.h>

vaddr_t uc;

static int boot_spd_symbs(struct cobj_header *h, spdid_t spdid, vaddr_t *spdid_addr, vaddr_t *heap_ptr_addr)
{
	unsigned int i, ras_index = 0;

	for (i = 0 ; i < h->nsymb ; i++) {
		struct cobj_symb *symb;

		symb = cobj_symb_get(h, i);
		assert(symb);
		if (COBJ_SYMB_UNDEF == symb->type) break;

		switch (symb->type) {
		case COBJ_SYMB_UCAP_TBL:
			if (cos_spd_cntl(COS_SPD_UCAP_TBL, spdid, symb->vaddr, 0)) assert(0);
			break;
		case COBJ_SYMB_UPCALL:
			if (cos_spd_cntl(COS_SPD_UPCALL_ADDR, spdid, symb->vaddr, 0)) assert(0);
			uc = symb->vaddr;
			break;
		case COBJ_SYMB_RAS_START:
		case COBJ_SYMB_RAS_END:
			if (cos_spd_cntl(COS_SPD_ATOMIC_SECT, spdid, symb->vaddr, ras_index++)) assert(0);
			break;
		case COBJ_SYMB_SPDID:
			*spdid_addr = symb->vaddr;
			break;
		case COBJ_SYMB_HEAPPTR:
			*heap_ptr_addr = symb->vaddr;
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

static vaddr_t boot_spd_end(struct cobj_header *h)
{
	struct cobj_sect *sect;
	int max_sect;

	max_sect = h->nsect-1;
	sect = cobj_sect_get(h, max_sect);
	
	return sect->vaddr + round_up_to_page(sect->bytes);
}

static int boot_spd_map(struct cobj_header *h, spdid_t spdid, vaddr_t spdid_addr, vaddr_t heap_ptr_addr)
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
			dsrc = cos_heap_ptr;
			cos_heap_ptr += PAGE_SIZE;
			if ((vaddr_t)dsrc != mman_get_page(cos_spd_id(), (vaddr_t)dsrc, 0)) assert(0);

			if (sect->flags & COBJ_SECT_ZEROS) {
				memset(dsrc, 0, PAGE_SIZE);
			} else {
				memcpy(dsrc, lsrc, page_left);
				if (page_left < PAGE_SIZE) memset(dsrc+page_left, 0, PAGE_SIZE - page_left);
			}

			/* Check if special symbols that need
			 * modification are in this page */
			boot_symb_reify(dsrc, dest_daddr, heap_ptr_addr, boot_spd_end(h));
			boot_symb_reify_16(dsrc, dest_daddr, spdid_addr, (u16_t)spdid);
			
			if (dest_daddr != (mman_alias_page(cos_spd_id(), (vaddr_t)dsrc, spdid, dest_daddr))) assert(0);

			lsrc += PAGE_SIZE;
			dest_daddr += PAGE_SIZE;
			left -= page_left;
		}
	}
	return 0;
}

static int boot_spd_reserve_caps(struct cobj_header *h, spdid_t spdid)
{
	if (cos_spd_cntl(COS_SPD_RESERVE_CAPS, spdid, h->ncap, 0)) assert(0);
	return 0;
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
		    cos_cap_cntl(COS_CAP_ACTIVATE, spdid, cap->cap_off, cap->dest_id)) assert(0);
	}

	return 0;
}

int boot_spd_thd(spdid_t spdid)
{
	struct cos_array *data;
	int new_thd;

	data = cos_argreg_alloc(sizeof(struct cos_array) + 3);
	assert(data);
	strcpy(&data->mem[0], "r1");
	data->sz = 3;
	if (0 > (new_thd = sched_create_thread(spdid, data))) assert(0);
	cos_argreg_free(data);
	
	return new_thd;
}

void cos_init(void *arg)
{
	struct cobj_header *h;
	spdid_t spdid;
	struct cobj_sect *sect;
	vaddr_t spdid_addr, heap_ptr_addr;

	h = (struct cobj_header*)cos_heap_ptr;
	cos_heap_ptr = (void*)round_up_to_page(cos_heap_ptr + h->size);
	printc("header %p, size %d, new heap %p\n", h, h->size, cos_heap_ptr);

	if ((spdid = cos_spd_cntl(COS_SPD_CREATE, 0, 0, 0)) == 0) assert(0);
	sect = cobj_sect_get(h, 0);
	if (cos_spd_cntl(COS_SPD_LOCATION, spdid, sect->vaddr, SERVICE_SIZE)) assert(0);

	if (boot_spd_symbs(h, spdid, &spdid_addr, &heap_ptr_addr)) assert(0);
	if (boot_spd_map(h, spdid, spdid_addr, heap_ptr_addr)) assert(0);
	if (boot_spd_reserve_caps(h, spdid)) assert(0);
	if (cos_spd_cntl(COS_SPD_ACTIVATE, spdid, 0, 0)) assert(0);
	if (boot_spd_caps(h, spdid)) assert(0);
	
	printc("booted spdid %d\n", spdid);

	if (-1 == boot_spd_thd(spdid)) assert(0);

	return;
}

void bin (void)
{
	sched_block(cos_spd_id(), 0);
}

