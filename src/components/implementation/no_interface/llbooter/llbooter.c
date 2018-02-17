#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>

#include "boot_deps.h"

#define USER_CAPS_SYMB_NAME "ST_user_caps"

#define MAX_DEPS (PAGE_SIZE/sizeof(struct deps))

struct deps {
	short int client, server;
};
struct deps deps_list[MAX_DEPS];
int          ndeps;
int          num_cobj;
int          resmgr_spdid;
int          root_spdid;

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
	int		  first = 1;
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

int
boot_spd_inv_cap_alloc(struct cobj_header *h, spdid_t spdid)
{
	struct cobj_cap *cap;
	struct usr_inv_cap inv_cap;
	int cap_offset;
	unsigned int i;

	for (i = 0; i < h->ncap ; i++) {

		cap = cobj_cap_get(h, i);
		assert(cap);

		/* 0 index of inv_cap array is special, so start at 1 */
		cap_offset = cap->cap_off + 1;

		/* Create the user cap for the undef symb */
		inv_cap = (struct usr_inv_cap) {
			.invocation_fn = (vaddr_t) cap->cstub,
			.service_entry_inst = (vaddr_t) cap->sstub,
			.invocation_count = cap->dest_id
		};

		new_comp_cap_info[spdid].ST_user_caps[cap_offset] = inv_cap;
	}
	return 0;
}

/*
 * Hacky specification until we have json parser working.
 * Root scheduler (not booter): "root_" prefix
 * Hierarchical schedulers: "<sched_no>_<parent_no>_" prefix, sched_no or parent_no is not related to spdid. it's really a static number from init string..
 *                          More importantly, the order in init string for hierarchical scheduling should be increasing order of levels.
 * 			    ID of root scheduler be == 0. Hierarchical scheduler numbers monotonically increasing starting from 1.
 * Non-scheduling components: "_<parent_no>_" prefix.??.
 * Resource manager: "resmgr" as its name!
 *
 * Thought: If SPDID namespace is created based on some hierarchical protocol then doing it here will be unnecessary. (I first need to find where SPDIDs are created though!)
 */
#define BOOT_ROOT_SCHED   "root_"
#define BOOT_SCHED_OFF    0
#define BOOT_PARENT_OFF   1
#define BOOT_NONSCHED_STR "_"
#define BOOT_RESMGR       "resmgr"
#define BOOT_DELIMITER    "_"

#define BOOT_NAME_MAX 32

static spdid_t
boot_comp_find_parent(spdid_t s)
{
	int i = 0;

	/* NOTE: only from objects parsed so far */
	for (; i <= num_cobj; i++) {
		if (new_comp_cap_info[i].sched_no == new_comp_cap_info[s].parent_no) return i;
	}

	return 0;
}

static void
boot_comp_name_parse(spdid_t s, const char *strname)
{
	struct comp_cap_info *cinfo = &new_comp_cap_info[s];
	struct comp_cap_info *bootcinfo = &new_comp_cap_info[0];
	char name[BOOT_NAME_MAX] = { '\0' };

#if 0
	cinfo->is_sched = 1;
	cinfo->parent_spdid = 0;

	return;
#endif
	char *tok;
	int count_parsed = 0;

	strncpy(name, strname, BOOT_NAME_MAX); 
	cinfo->is_sched = -1;
	cinfo->childid_bitf = 0;
	if (name[0] == '_') {
		/* TODO: */
		/* set it to be non-scheduler */
		/* set parent to be none: don't care who uses or runs it for now! */
		cinfo->is_sched  = 0;
		cinfo->sched_no  = -1;
		cinfo->parent_no = 0;

		count_parsed = 1;
	} else if (strncmp(name, BOOT_ROOT_SCHED, 5) == 0) {
		/* set it to be the root scheduler */
		/* set it's parent to be llbooter! */
		cinfo->is_sched = 1;
		cinfo->parent_spdid = 0;
		cinfo->sched_no = 2;
		root_spdid = s;

		bootcinfo->childid_bitf |= (1 << s);
		return;
	} else if (strcmp(name, BOOT_RESMGR) == 0) {
		/* llbooter creates init thread and lets it initialize. (for my impl: should not run after boot-up */
		/* set it's parent to be llbooter */
		resmgr_spdid = s;
		cinfo->is_sched  = 0;
		cinfo->sched_no  = 1;
		cinfo->parent_no = 0;
		cinfo->parent_spdid = 0;

		bootcinfo->childid_bitf |= (1 << s);
		return;
	}

	tok = strtok(name, BOOT_DELIMITER);
	while (tok != NULL) {
		switch (count_parsed) {
		case BOOT_SCHED_OFF:
		{
			cinfo->sched_no = atoi(tok);
			cinfo->is_sched = 1;
			break;
		}
		case BOOT_PARENT_OFF:
		{
			struct comp_cap_info *parentcinfo = NULL;

			cinfo->parent_no    = atoi(tok);
			cinfo->parent_spdid = boot_comp_find_parent(s);

			if (cinfo->parent_spdid <= num_cobj) {
				parentcinfo = &new_comp_cap_info[cinfo->parent_spdid];
				assert(s);
				parentcinfo->childid_bitf |= (1 << (s-1));
			}
			break;
		}
		default: break;
		}

		count_parsed ++;
		tok = strtok(NULL, BOOT_DELIMITER);
		if (cinfo->is_sched == 0) break;
		if (count_parsed == 2) break;
	}

	assert(count_parsed == 2);

	/* FIXME: should just work with carefully crafted names. no consistency checks here! */
	return;
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

		assert(spdid != 0);

		sect                                = cobj_sect_get(h, 0);
		new_comp_cap_info[spdid].addr_start = sect->vaddr;
		boot_comp_name_parse(spdid, h->name);
		boot_compinfo_init(spdid, &ct, &pt, sect->vaddr);

		if (boot_spd_symbs(h, spdid, &ci, &new_comp_cap_info[spdid].vaddr_user_caps)) BUG();
		if (boot_spd_inv_cap_alloc(h, spdid)) BUG();
		if (boot_comp_map(h, spdid, ci, pt)) BUG();

		boot_newcomp_create(spdid, new_comp_cap_info[spdid].compinfo);
		printc("Comp %d (%s) created @ %x!\n", h->id, h->name, sect->vaddr);
	}

	return;
}

void
boot_init_ndeps(int num_cobj)
{
	int i = 0;

	printc("MAX DEPS: %d\n", MAX_DEPS);
	for (i = 0; i < deps_list[i].server; i++) {
//		if (deps_list[i].client != 0) printc("client: %d, server: %d \n", deps_list[i].client, deps_list[i].server);
	}

	printc("ndeps: %d\n", ndeps);
	ndeps = i;
}

void
boot_child_info(void)
{
	int i = 0;

	for (; i <= num_cobj; i++) {
		printc("Component %d => child bitmap %llx\n", i, new_comp_cap_info[i].childid_bitf);
	}
}

void
cos_init(void)
{
	struct cobj_header *h;

	printc("Booter for new kernel\n");

	resmgr_spdid = 0;
	root_spdid = 0;

	h        = (struct cobj_header *)cos_comp_info.cos_poly[0];
	num_cobj = (int)cos_comp_info.cos_poly[1];

	assert(num_cobj <= MAX_NUM_SPDS);
	memset(new_comp_cap_info, 0, sizeof(struct comp_cap_info) * (MAX_NUM_SPDS + 1));

	new_comp_cap_info[0].defci    = cos_defcompinfo_curr_get();
	new_comp_cap_info[0].compinfo = cos_compinfo_get(new_comp_cap_info[0].defci);

	//deps = (struct deps *)cos_comp_info.cos_poly[2];
	memcpy(deps_list, (struct deps *)cos_comp_info.cos_poly[2], PAGE_SIZE);
	boot_init_ndeps(num_cobj);

	init_args = (struct component_init_str *)cos_comp_info.cos_poly[3];
	init_args++;

	boot_sched = (unsigned int *)cos_comp_info.cos_poly[4];
	boot_init_sched();

	printc("num cobjs: %d\n", num_cobj);
	boot_find_cobjs(h, num_cobj);
	boot_bootcomp_init();
	boot_create_cap_system();
	boot_child_info();

	boot_done();
}
