#include <cos_types.h>
#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <stdlib.h>

#include "boot_deps.h"

#define USER_CAPS_SYMB_NAME "ST_user_caps"

#define MAX_DEPS (PAGE_SIZE/sizeof(struct deps))

struct deps {
	short int client, server;
};
struct deps deps_list[MAX_DEPS];
int          ndeps;
int          num_cobj;
spdid_t      capmgr_spdid;
spdid_t      root_spdid[NUM_CPU];

/*Component init info*/
#define INIT_STR_SZ 52
struct component_init_str {
	unsigned int spdid, schedid;
	int          startup;
	char         init_str[INIT_STR_SZ];
} __attribute__((packed));

struct component_init_str *init_args, *boot_init_args;

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
		PRINTLOG(PRINT_DEBUG, "cobj %s:%d found at %p:%x, -> %x\n", h->name, h->id, hs[i - 1], size,
		       cobj_sect_get(hs[i - 1], 0)->vaddr);

		end   = start + round_up_to_cacheline(size);
		hs[i] = h = (struct cobj_header *)end;
		start     = end;
	}

	hs[n] = NULL;
	PRINTLOG(PRINT_DEBUG, "cobj %s:%d found at %p:%x -> %x\n", hs[n - 1]->name, hs[n - 1]->id, hs[n - 1], hs[n-1]->size,
	       cobj_sect_get(hs[n - 1], 0)->vaddr);

}

static int
boot_comp_map_memory(struct cobj_header *h, spdid_t spdid)
{
	int               i;
	int		  first = 1;
	vaddr_t           dest_daddr, prev_map = 0, map_daddr;
	int               n_pte = 1;
	struct cobj_sect *sect = cobj_sect_get(h, 0);
	struct comp_cap_info *spdinfo = boot_spd_compcapinfo_get(spdid);

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
				spdinfo->vaddr_mapped_in_booter = boot_deps_map_sect(spdid, &map_daddr);
				first = 0;
			} else {
				boot_deps_map_sect(spdid, &map_daddr);
			}
			assert(dest_daddr == map_daddr);
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
	int                               i, ret;
	struct cos_component_information *ci;
	struct comp_cap_info             *spdinfo = boot_spd_compcapinfo_get(spdid);

	assert(symb_addr == round_to_page(symb_addr));
	ci = (struct cos_component_information *)(mem);

	if (!ci->cos_heap_ptr) ci->cos_heap_ptr = heap_val;

	ci->cos_this_spd_id = spdid;
	ci->init_string[0]  = '\0';

	for (i = 0; init_args[i].spdid; i++) {
		char *start, *end;
		int   len, ret;

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

	/* returns full-bitmap(all bits set to 1) if not specified in the runscript */
	ret = cos_args_cpubmp(spdinfo->cpu_bitmap, ci->init_string);
	assert(ret == 0);
	PRINTLOG(PRINT_DEBUG, "Comp %u init-string:%s, init-cpu_bitmap:", spdid, ci->init_string);
	for (i = NUM_CPU-1; i >= 0; i--) printc("%d", bitmap_check(spdinfo->cpu_bitmap, i) ? 1 : 0);
	printc("\n");

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
	struct comp_cap_info             *spdinfo = boot_spd_compcapinfo_get(spdid);

	start_addr = (char *)(spdinfo->vaddr_mapped_in_booter);
	init_daddr = cobj_sect_get(h, 0)->vaddr;

	for (i = 0; i < h->nsect; i++) {
		struct cobj_sect     *sect;
		vaddr_t               dest_daddr;
		char                 *lsrc;
		int                   left;
		struct comp_cap_info *hinfo = NULL;

		sect = cobj_sect_get(h, i);
		/* virtual address in the destination address space */
		dest_daddr = sect->vaddr;
		/* where we're copying from in the cobj */
		lsrc = cobj_sect_contents(h, i);
		/* how much is left to copy? */
		left = cobj_sect_size(h, i);

		/* Initialize memory. */
		if (!(sect->flags & COBJ_SECT_KMEM)) {
			if (sect->flags & COBJ_SECT_ZEROS) {
				memset(start_addr + (dest_daddr - init_daddr), 0, left);
			} else {
				memcpy(start_addr + (dest_daddr - init_daddr), lsrc, left);
			}
		}

		if (sect->flags & COBJ_SECT_CINFO) {
			assert((left % PAGE_SIZE) == 0);
			assert(comp_info == (dest_daddr + (((left/PAGE_SIZE)-1)*PAGE_SIZE)));
			boot_process_cinfo(h, spdid, boot_spd_end(h), start_addr + (comp_info - init_daddr), comp_info);
			ci = (struct cos_component_information *)(start_addr + (comp_info - init_daddr));

			hinfo = boot_spd_compcapinfo_get(h->id);
			hinfo->upcall_entry = ci->cos_upcall_entry;
		}

	}

	return 0;
}

int
boot_comp_map(struct cobj_header *h, spdid_t spdid, vaddr_t comp_info)
{
	boot_comp_map_memory(h, spdid);
	boot_comp_map_populate(h, spdid, comp_info);

	return 0;
}

static void
boot_init_sched(void)
{
	int i;

	for (i = 0; i < MAX_NUM_SPDS; i++) schedule[cos_cpuid()][i] = 0;
	sched_cur[cos_cpuid()] = 0;
}

int
boot_spd_inv_cap_alloc(struct cobj_header *h, spdid_t spdid)
{
	struct comp_cap_info *spdinfo = boot_spd_compcapinfo_get(spdid);
	struct cobj_cap *cap;
	struct usr_inv_cap inv_cap;
	int cap_offset;
	size_t i;

	for (i = 0 ; i < h->ncap ; i++) {
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

		spdinfo->ST_user_caps[cap_offset] = inv_cap;
	}
	return 0;
}

#define BOOT_ROOT_SCHED   "boot"
#define BOOT_CAPMGR       "capmgr"

static void
boot_comp_name_parse(spdid_t s, const char *name)
{
	struct comp_sched_info *si = boot_spd_comp_schedinfo_get(s);

	if (strcmp(name, BOOT_ROOT_SCHED) == 0) {
		si->flags |= COMP_FLAG_SCHED;
		root_spdid[cos_cpuid()] = s;
	} else if (!capmgr_spdid && strcmp(name, BOOT_CAPMGR) == 0) {
		capmgr_spdid = s;
		si->flags |= COMP_FLAG_SCHED;
		si->flags |= COMP_FLAG_CAPMGR;
	}

	return;
}

static void
boot_comp_preparse_name(void)
{
	unsigned int i;

	for (i = 0; hs[i] != NULL; i++) {
		struct cobj_header *h;
		spdid_t             spdid;
		struct comp_sched_info *spdsi, *schedspdsi;

		h     = hs[i];
		spdid = h->id;

		assert(spdid != 0);

		boot_comp_name_parse(spdid, h->name);

		if (!(h->flags)) continue;
		spdsi      = boot_spd_comp_schedinfo_get(spdid);
		schedspdsi = boot_spd_comp_schedinfo_get(spdsi->parent_spdid);

		assert((spdsi->flags & COMP_FLAG_CAPMGR) == 0);
		assert(schedspdsi->flags & COMP_FLAG_SCHED);
		spdsi->flags |= COMP_FLAG_SCHED;
	}

	PRINTLOG(PRINT_DEBUG, "Capability manager component[=%u] %s!\n", capmgr_spdid, capmgr_spdid ? "found" : "not found");
	PRINTLOG(PRINT_DEBUG, "Root scheduler component[=%u] %s!\n", root_spdid[cos_cpuid()], root_spdid[cos_cpuid()] ? "found" : "not found");
}

static void
boot_create_cap_system(void)
{
	unsigned int i;

	boot_comp_preparse_name();

	if (cos_cpuid() == INIT_CORE) {
		for (i = 0; hs[i] != NULL; i++) {
			struct comp_cap_info *spdinfo;
			struct cobj_header   *h;
			struct cobj_sect     *sect;
			captblcap_t           ct;
			pgtblcap_t            pt;
			spdid_t               spdid;
			vaddr_t               ci = 0;

			h     = hs[i];
			spdid = h->id;

			assert(spdid != 0);
			spdinfo = boot_spd_compcapinfo_get(spdid);

			sect                = cobj_sect_get(h, 0);
			spdinfo->addr_start = sect->vaddr;
			boot_compinfo_init(spdid, &ct, &pt, sect->vaddr);

			if (boot_spd_symbs(h, spdid, &ci, &(spdinfo->vaddr_user_caps))) BUG();
			if (boot_spd_inv_cap_alloc(h, spdid)) BUG();
			if (boot_comp_map(h, spdid, ci)) BUG();

			boot_newcomp_create(spdid, boot_spd_compinfo_get(spdid));
			PRINTLOG(PRINT_DEBUG, "Comp %d (%s) created @ %x!\n", h->id, h->name, sect->vaddr);
		}

		/*
		 * create invocations only after all components are created,
		 * to be able to resolve forward dependencies.
		 */
		for (i = 0; hs[i] != NULL; i++) {
			struct cobj_header *h;
			spdid_t             spdid;

			h     = hs[i];
			spdid = h->id;

			assert(spdid != 0);
			boot_newcomp_sinv_alloc(spdid);
			PRINTLOG(PRINT_DEBUG, "Comp %d (%s) undefined symbols resolved!\n", h->id, h->name);
		}
	} else {
		for (i = 0; hs[i] != NULL; i++) {
			struct cobj_header *h;
			spdid_t             spdid;

			h     = hs[i];
			spdid = h->id;

			assert(spdid != 0);
			boot_sched_caps_init(spdid);
		}
	}

	return;
}

void
boot_child_info_print(void)
{
	int i;

	for (i = 0; i <= num_cobj; i++) {
		struct comp_sched_info *si = boot_spd_comp_schedinfo_get(i);
		int j;

		PRINTLOG(PRINT_DEBUG, "Component %d => child", i);
		for (j = 0; j < (int)MAX_NUM_COMP_WORDS; j++) printc(" bitmap[%d]: %04x", j, si->child_bitmap[j]);
		printc("\n");
	}
}

void
boot_parse_init_args(void)
{
	struct comp_sched_info *cap_si = boot_spd_comp_schedinfo_get(capmgr_spdid);
	int i = 1;

	for (; i <= num_cobj; i++) {
		spdid_t spdid = boot_init_args[i].spdid, schedid = boot_init_args[i].schedid;
		struct comp_sched_info *spdsi      = boot_spd_comp_schedinfo_get(spdid);
		struct comp_sched_info *schedspdsi = boot_spd_comp_schedinfo_get(schedid);

		assert(schedspdsi);
		assert(spdsi);
		spdsi->parent_spdid = schedid;
		bitmap_set(schedspdsi->child_bitmap, spdid - 1);
		schedspdsi->flags |= COMP_FLAG_SCHED;
		schedspdsi->num_child++;
	}

	if (capmgr_spdid && !cap_si->flags) {
		cap_si->flags |= COMP_FLAG_SCHED;
		cap_si->flags |= COMP_FLAG_CAPMGR;
	}
}

void
boot_comp_capinfo_init(void)
{
	int i;

	memset(comp_schedinfo[cos_cpuid()], 0, sizeof(struct comp_sched_info) * (MAX_NUM_SPDS + 1));

	for (i = 1; i <= MAX_NUM_SPDS; i++) {
		struct comp_cap_info *spdinfo = boot_spd_compcapinfo_get(i);

		(spdinfo->schedinfo)[cos_cpuid()] = &(comp_schedinfo[cos_cpuid()][i]);
	}
}

static volatile int init_core_alloc_done = 0, core_init_done[NUM_CPU] = { 0 };

void
cos_init(void)
{
	struct cobj_header *h;
	int cycs = 0, i;

	cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

        PRINTLOG(PRINT_DEBUG, "%d cycles per microsecond\n", cycs);

	if (cos_cpuid() == INIT_CORE) {
		capmgr_spdid = 0;
		memset(root_spdid, 0, sizeof(int) * NUM_CPU);
		memset(new_comp_cap_info, 0, sizeof(struct comp_cap_info) * (MAX_NUM_SPDS));

		h        = (struct cobj_header *)cos_comp_info.cos_poly[0];
		num_cobj = (int)cos_comp_info.cos_poly[1];

		PRINTLOG(PRINT_DEBUG, "Low-level boot-up start\n");
		PRINTLOG(PRINT_DEBUG, "num cobjs: %d\n", num_cobj);
		assert(num_cobj <= MAX_NUM_SPDS);

		boot_init_args = init_args = (struct component_init_str *)cos_comp_info.cos_poly[3];
		boot_init_sched();
		boot_comp_capinfo_init();
		boot_parse_init_args();
		init_args++;

		boot_find_cobjs(h, num_cobj);
		boot_bootcomp_init();
		boot_create_cap_system();
		boot_child_info_print();
		core_init_done[cos_cpuid()] = 1;

		for (i = 1; i < NUM_CPU; i++) {
			while (!core_init_done[i]) ;
		}

		/* All cores initialized. Create untyped space for memory manager. */
		if (capmgr_spdid) {
			boot_capmgr_mem_alloc();
		} else {
			for (i = 1; i <= num_cobj; i++) {
				boot_comp_mem_alloc(i);
			}
		}
		init_core_alloc_done = 1;

		boot_done();
		boot_root_sched_run();
	} else {
		while (!core_init_done[INIT_CORE]) ;

		PRINTLOG(PRINT_DEBUG, "Low-level boot-up start\n");
		boot_init_sched();
		boot_comp_capinfo_init();
		boot_parse_init_args();
		boot_bootcomp_init();
		boot_comp_preparse_name();
		boot_create_cap_system();
		boot_child_info_print();
		core_init_done[cos_cpuid()] = 1;

		while (!init_core_alloc_done) ;
		boot_done();
		boot_root_sched_run();
	}

	PRINTLOG(PRINT_WARN, "Booter spinning!\n");
	SPIN();
}
