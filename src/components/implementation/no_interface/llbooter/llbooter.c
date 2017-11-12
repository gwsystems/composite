#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>

#include "boot_deps.h"

#define USER_CAPS_SYMB_NAME "ST_user_caps"

struct deps {
	short int client, server;
};
struct deps *deps;
int          ndeps;

/*Component init info*/
#define INIT_STR_SZ 52
struct component_init_str {
	unsigned int spdid, schedid;
	int          startup;
	char         init_str[INIT_STR_SZ];
} __attribute__((packed));

struct component_init_str *init_args;

unsigned int *boot_sched;

static void
boot_find_cobjs(struct cobj_header *h, int n)
{
	int     i;
	vaddr_t start, end;

	start = (vaddr_t)h;
	hs[0] = h;

	for (i = 1; i < n; i++) {
		int j = 0, size = 0, tot = 0;

		size = h->size;
		for (j = 0; j < (int)h->nsect; j++) {
			tot += cobj_sect_size(h, j);
		}
		printc("cobj %s:%d found at %p:%x, -> %x\n", h->name, h->id, hs[i - 1], size,
		       cobj_sect_get(hs[i - 1], 0)->vaddr);

		end   = start + round_up_to_cacheline(size);
		hs[i] = h = (struct cobj_header *)end;
		start     = end;
	}

	hs[n] = NULL;
	printc("cobj %s:%d found at %p -> %x\n", hs[n - 1]->name, hs[n - 1]->id, hs[n - 1],
	       cobj_sect_get(hs[n - 1], 0)->vaddr);
	
}

static int
boot_comp_map_memory(struct cobj_header *h, spdid_t spdid, pgtblcap_t pt)
{
	int               i;
	int 		  first = 1;
	vaddr_t           dest_daddr, prev_map = 0;
	int               n_pte = 1;
	struct cobj_sect *sect = cobj_sect_get(h, 0);

	boot_comp_pgtbl_expand(n_pte, pt, sect->vaddr, h);

	/* We'll map the component into booter's heap. */

	for (i = 0; i < (int)h->nsect; i++) {
		int left;

		sect = cobj_sect_get(h, i);

		dest_daddr = sect->vaddr;
		left       = cobj_sect_size(h, i);

		/* previous section overlaps with this one, don't remap! */
		if (round_to_page(dest_daddr) == prev_map) {
			left -= (prev_map + PAGE_SIZE - dest_daddr);
			dest_daddr = prev_map + PAGE_SIZE;
		}

		while (left > 0) {
			if (first) { 
				new_comp_cap_info[spdid].vaddr_mapped_in_booter = boot_deps_map_sect(spdid, dest_daddr);
				first = 0;
			} else {
				boot_deps_map_sect(spdid, dest_daddr);
			}
			prev_map = dest_daddr;
			dest_daddr += PAGE_SIZE;
			left -= PAGE_SIZE;
		}
	}
	
	return 0;
}


static vaddr_t
boot_spd_end(struct cobj_header *h)
{
	struct cobj_sect *sect;
	int               max_sect;

	max_sect = h->nsect - 1;
	sect     = cobj_sect_get(h, max_sect);

	return sect->vaddr + round_up_to_page(sect->bytes);
}

struct cobj_symb *
cobj_find_symb(const char *symbol_name, int cobj) 
{
	unsigned int j = 0;
	for (j = 0; j < hs[cobj]->nsymb; j++) {
		struct cobj_symb *symb;

		symb = cobj_symb_get(hs[cobj], j);
		assert(symb);

		if (!strcmp(symb->name, symbol_name) && symb->type == COBJ_SYMB_EXPORTED) {
			return symb;
		}
	}		
	return NULL;
}

vaddr_t
boot_find_inv_symb_addr(struct cobj_symb *undefsymb) {
	
	unsigned int i;
	vaddr_t symb_addr;	
	struct cobj_symb *symb;
	char jumper_symb[25];
	sprintf(jumper_symb, "%s_inv", undefsymb->name);

	for (i = 0; hs[i] != NULL; i++) {
		symb = cobj_find_symb(jumper_symb, i);
		if (symb) break;	
	}

	assert(symb);
	symb_addr = symb->vaddr;

	return symb_addr;
}

int
boot_link_symbs(struct cobj_header *h, spdid_t spdid)
{
	int i = 0;
	struct cobj_symb *symb;
	vaddr_t symb_addr;

	for (i = 0; i < (int)h->nsymb; i++) {

		symb = cobj_symb_get(h, i);
		assert(symb);
	
		if (COBJ_SYMB_UNDEF == symb->type) {
			struct cobj_symb *ipc_client_symb;
			struct usr_inv_cap cap;	
			ipc_client_symb = cobj_find_symb("SS_ipc_client_marshal_args", spdid-1);
			symb_addr = ipc_client_symb->vaddr;
		
			/* Create the user cap for the undef symb */
			cap = (struct usr_inv_cap) {
				.invocation_fn = symb_addr
			};	
			
			new_comp_cap_info[spdid].ST_user_caps[symb->user_caps_offset] = cap;	
			new_comp_cap_info[spdid].ST_user_caps[symb->user_caps_offset].service_entry_inst = boot_find_inv_symb_addr(symb);
		}
	}

	return 0;
}

int
boot_spd_symbs(struct cobj_header *h, spdid_t spdid, vaddr_t *comp_info, vaddr_t *user_caps)
{
	int i = 0;

	for (i = 0; i < (int)h->nsymb; i++) {
		struct cobj_symb *symb;

		symb = cobj_symb_get(h, i);
		assert(symb);
	
		switch (symb->type) {
		case COBJ_SYMB_COMP_INFO:
			*comp_info = symb->vaddr;
			break;
		case COBJ_SYMB_EXPORTED:
			break;	
		case COBJ_SYMB_COMP_PLT:
			*user_caps = symb->vaddr;
			break;	
		default:
			break;
		}
	}
	return 0;
}

static int
boot_process_cinfo(struct cobj_header *h, spdid_t spdid, vaddr_t heap_val, char *mem, vaddr_t symb_addr)
{
	int                               i;
	struct cos_component_information *ci;

	assert(symb_addr == round_to_page(symb_addr));
	ci = (struct cos_component_information *)(mem);

	if (!ci->cos_heap_ptr) ci->cos_heap_ptr = heap_val;

	ci->cos_this_spd_id = spdid;
	ci->init_string[0]  = '\0';

	for (i = 0; init_args[i].spdid; i++) {
		char *start, *end;
		int   len;

		if (init_args[i].spdid != spdid) continue;

		start = strchr(init_args[i].init_str, '\'');
		if (!start) break;
		start++;
		end = strchr(start, '\'');
		if (!end) break;
		len = (int)(end - start);
		memcpy(&ci->init_string[0], start, len);
		ci->init_string[len] = '\0';
	}

	return 1;
}

static int
boot_comp_map_populate(struct cobj_header *h, spdid_t spdid, vaddr_t comp_info)
{
	unsigned int i;
	/* Where are we in the actual component's memory in the booter? */
	char *start_addr;
	/* Where are we in the destination address space? */
	vaddr_t                           init_daddr;
	struct cos_component_information *ci;

	start_addr = (char *)(new_comp_cap_info[spdid].vaddr_mapped_in_booter);
	init_daddr = cobj_sect_get(h, 0)->vaddr;

	int total = 0;
	for (i = 0; i < h->nsect; i++) {
		struct cobj_sect *sect;
		vaddr_t           dest_daddr;
		char *            lsrc;
		int               left;

		sect = cobj_sect_get(h, i);
		/* virtual address in the destination address space */
		dest_daddr = sect->vaddr;
		/* where we're copying from in the cobj */
		lsrc = cobj_sect_contents(h, i);
		/* how much is left to copy? */
		left = cobj_sect_size(h, i);
		total += left;

		/* Initialize memory. */
		if (!(sect->flags & COBJ_SECT_KMEM)) {
			if (sect->flags & COBJ_SECT_ZEROS) {
				memset(start_addr + (dest_daddr - init_daddr), 0, left);
			} else {
				memcpy(start_addr + (dest_daddr - init_daddr), lsrc, left);
			}
		}

		if (sect->flags & COBJ_SECT_CINFO) {
			assert(left == PAGE_SIZE);
			assert(comp_info == dest_daddr);
			boot_process_cinfo(h, spdid, boot_spd_end(h), start_addr + (comp_info - init_daddr), comp_info);
			ci = (struct cos_component_information *)(start_addr + (comp_info - init_daddr));
			new_comp_cap_info[h->id].upcall_entry = ci->cos_upcall_entry;
		}
	
	}

	return 0;
}

int
boot_comp_map(struct cobj_header *h, spdid_t spdid, vaddr_t comp_info, pgtblcap_t pt)
{
	boot_comp_map_memory(h, spdid, pt);
	boot_comp_map_populate(h, spdid, comp_info);
	return 0;
}

static void
boot_init_sched(void)
{
	int i;

	for (i = 0; i < MAX_NUM_SPDS; i++) schedule[i] = 0;
	sched_cur = 0;
}

static void
boot_create_cap_system(void)
{
	unsigned int i;

	for (i = 0; hs[i] != NULL; i++) {
		struct cobj_header *h;
		struct cobj_sect *  sect;
		captblcap_t         ct;
		pgtblcap_t          pt;
		spdid_t             spdid;
		vaddr_t             ci = 0;

		h     = hs[i];
		spdid = h->id;

		sect                                = cobj_sect_get(h, 0);
		new_comp_cap_info[spdid].addr_start = sect->vaddr;
		boot_compinfo_init(spdid, &ct, &pt, sect->vaddr);
		
		if (boot_spd_symbs(h, spdid, &ci, &new_comp_cap_info[spdid].vaddr_user_caps)) BUG();
		if (boot_link_symbs(h, spdid)) BUG();
		if (boot_comp_map(h, spdid, ci, pt)) BUG();

		boot_newcomp_create(spdid, new_comp_cap_info[spdid].compinfo);

		printc("\nComp %d (%s) created @ %x!\n\n", h->id, h->name, sect->vaddr);
	}

	return;
}

void
boot_init_ndeps(void)
{
	int i;

	for (i = 0; deps[i].server; i++)
		;
	ndeps = i;
}

void
cos_init(void)
{
	struct cobj_header *h;
	int                 num_cobj;

	printc("Booter for new kernel\n");

	h        = (struct cobj_header *)cos_comp_info.cos_poly[0];
	num_cobj = (int)cos_comp_info.cos_poly[1];

	deps = (struct deps *)cos_comp_info.cos_poly[2];
	boot_init_ndeps();

	init_args = (struct component_init_str *)cos_comp_info.cos_poly[3];
	init_args++;

	boot_sched = (unsigned int *)cos_comp_info.cos_poly[4];
	boot_init_sched();

	printc("num cobjs: %d\n", num_cobj);
	boot_find_cobjs(h, num_cobj);
	boot_bootcomp_init();
	boot_create_cap_system();

	boot_done();
}
