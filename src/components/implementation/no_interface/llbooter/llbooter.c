#include <cos_types.h>
#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <stdlib.h>
#include <initargs.h>

#include "boot_deps.h"

#define USER_CAPS_SYMB_NAME "ST_user_caps"

#define MAX_DEPS (PAGE_SIZE/sizeof(struct deps))

struct deps {
	short int client, server;
};
struct deps deps_list[MAX_DEPS];
int          ndeps;
int          num_cobj;
int          capmgr_spdid;
int          root_spdid;

/*Component init info*/
#define INIT_STR_SZ 52
struct component_init_str {
	unsigned int spdid, schedid;
	int          startup;
	char         init_str[INIT_STR_SZ];
} __attribute__((packed));

struct component_init_str *init_args;

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

	for (i = 0; i < MAX_NUM_SPDS; i++) schedule[i] = 0;
	sched_cur = 0;
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
			.invocation_fn = (vaddr_t)cap->cstub,
			.data          = (void *)cap->sstub,
//			.invocation_count = cap->dest_id
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
		root_spdid = s;
	} else if (strcmp(name, BOOT_CAPMGR) == 0) {
		capmgr_spdid = s;
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
	PRINTLOG(PRINT_DEBUG, "Root scheduler component[=%u] %s!\n", root_spdid, root_spdid ? "found" : "not found");
}

static void
boot_create_cap_system(void)
{
	unsigned int i;

	boot_comp_preparse_name();

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
	boot_capmgr_mem_alloc();

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
	int i = 1;

	for (; i <= num_cobj; i++) {
		spdid_t spdid = init_args[i].spdid, schedid = init_args[i].schedid;
		struct comp_sched_info *spdsi      = boot_spd_comp_schedinfo_get(spdid);
		struct comp_sched_info *schedspdsi = boot_spd_comp_schedinfo_get(schedid);

		spdsi->parent_spdid = schedid;
		bitmap_set(schedspdsi->child_bitmap, spdid - 1);
		schedspdsi->flags |= COMP_FLAG_SCHED;
		schedspdsi->num_child++;
	}
}

void
boot_comp_capinfo_init(void)
{
	int i;

	memset(new_comp_cap_info, 0, sizeof(struct comp_cap_info) * (MAX_NUM_SPDS));
	memset(comp_schedinfo, 0, sizeof(struct comp_sched_info) * (MAX_NUM_SPDS + 1));

	for (i = 1; i <= MAX_NUM_SPDS; i++) {
		struct comp_cap_info *spdinfo = boot_spd_compcapinfo_get(i);

		spdinfo->schedinfo = &comp_schedinfo[i];
	}
}

#include <elf_loader.h>
#include <cos_defkernel_api.h>

#ifndef BOOTER_MAX_SINV
#define BOOTER_MAX_SINV 256
#endif
#ifndef INITARGS_MAX_PATHNAME
#define INITARGS_MAX_PATHNAME 512
#endif

typedef enum {
	CRT_COMP_SCHED       = 1, 	/* is this a scheduler? */
	CRT_COMP_DELEG       = 1<<1,	/* does this component require delegating management capabilities to it? */
	CRT_COMP_DERIVED     = 1<<2, 	/* derived from another component */
	CRT_COMP_SCHED_DELEG = 1<<4,	/* is the system thread initialization delegated to this component? */
} crt_comp_flags_t;

struct crt_comp {
	crt_comp_flags_t flags;
	char *name;
	compid_t id;
	vaddr_t entry_addr;
	struct crt_comp *sched_parent; /* iff flags & CRT_COMP_SCHED */

	struct crt_sinv *sinvs;
	int n_sinvs;

	struct elf_hdr *elf_hdr;
	struct cos_defcompinfo comp_res;
};

void
crt_comp_init(struct crt_comp *comp, char *name, compid_t id)
{
	*comp = (struct crt_comp) {
		.name = name,
		.id   = id,
	};
}

void
crt_comp_elf(struct crt_comp *c, struct elf_hdr *elf_hdr)
{
	struct cos_compinfo *ci = cos_compinfo_get(&c->comp_res);

	c->elf_hdr    = elf_hdr;
	c->entry_addr = elf_entry_addr(elf_hdr);
}

/*
 * Actually create the process including all the resource tables, and
 * memory.
 *
 * Return 0 on success, -errno on failure -- either of elf parsing, or
 * of memory allocation.
 */
int
crt_comp_construct(struct crt_comp *c)
{
	vaddr_t ro_addr, rw_addr;
	size_t  ro_sz,   rw_sz, data_sz, bss_sz, tot_sz;
	char   *ro_src, *data_src, *mem;
	int     ret;
	struct cos_compinfo *ci      = cos_compinfo_get(&c->comp_res);
	struct cos_compinfo *root_ci = cos_compinfo_get(cos_defcompinfo_curr_get());

	assert(root_ci);
	if (!c->elf_hdr) return -1;

	if (elf_load_info(c->elf_hdr, &ro_addr, &ro_sz, &ro_src, &rw_addr, &data_sz, &data_src, &bss_sz)) return -1;

	ret = cos_compinfo_alloc(ci, ro_addr, BOOT_CAPTBL_FREE, c->entry_addr, boot_spd_compinfo_curr_get());
	assert(!ret);

	tot_sz = round_up_to_page(ro_sz) + round_up_to_page(data_sz + bss_sz);
	mem = cos_page_bump_allocn(ci, tot_sz);
	assert(mem);

	memcpy(mem, ro_src, ro_sz);
	memcpy(mem + ro_sz, data_src, data_sz);
	memset(mem + ro_sz + data_sz, 0, bss_sz);

	/* FIXME: separate map of RO and RW */
	if (ro_addr != cos_mem_aliasn(ci, root_ci, (vaddr_t)mem, tot_sz)) return -1;

	return 0;
}

struct crt_sinv {
	char *name;
	struct crt_comp *server, *client;
	vaddr_t c_fn_addr, c_ucap_addr;
	vaddr_t s_fn_addr;
};

void
crt_sinv_init(struct crt_sinv *sinv, char *name, struct crt_comp *server, struct crt_comp *client,
	      vaddr_t c_fn_addr, vaddr_t c_ucap_addr, vaddr_t s_fn_addr)
{
	*sinv = (struct crt_sinv) {
		.name        = name,
		.server      = server,
		.client      = client,
		.c_fn_addr   = c_fn_addr,
		.c_ucap_addr = c_ucap_addr,
		.s_fn_addr   = s_fn_addr
	};

	return;
}

/* Booter's additive information about the component */
struct boot_comp {
	struct crt_comp comp;
};
static struct boot_comp boot_comps[MAX_NUM_COMPS];
/* All synchronous invocations */
static struct crt_sinv  boot_sinvs[BOOTER_MAX_SINV];

static void
comps_init(void)
{
	struct initargs comps, curr;
	struct initargs_iter i;
	int cont, ret, j;
	int comp_idx = 0, sinv_idx = 0;

	ret = args_get_entry("components", &comps);
	assert(!ret);

	printc("Components (and initialization schedule):\n");
	for (cont = args_iter(&comps, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		int keylen;
		int   idx  = atoi(args_key(&curr, &keylen));
		char *name = args_value(&curr);
		struct crt_comp *comp;

		assert(idx < MAX_NUM_COMPS && idx >= 0);
		assert(name);

		comp = &boot_comps[idx].comp;
		crt_comp_init(comp, name, idx);

		printc("\t%d: %s\n", idx, name);
	}

	ret = args_get_entry("binaries", &comps);
	assert(!ret);

	for (j = 0 ; boot_comps[j].comp.name ; j++) {
		struct elf_hdr *elf;
		char path[INITARGS_MAX_PATHNAME];
		struct crt_comp *c = &boot_comps[j].comp;
		char *root = "binaries/";
		int   len  = strlen(root);

		strcpy(path, root);
		strncat(path, c->name, INITARGS_MAX_PATHNAME - len);

		crt_comp_elf(c, (struct elf_hdr *)args_get(path));
	}

	printc("Binaries (%d):\n", args_len(&comps));
	for (cont = args_iter(&comps, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		int keylen;

		printc("\t%s @ 0x%p\n", args_key(&curr, &keylen), args_value(&curr));
	}

	ret = args_get_entry("sinvs", &comps);
	assert(!ret);

	printc("Synchronous invocations (%d):\n", args_len(&comps));
	for (cont = args_iter(&comps, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		struct crt_sinv *sinv;
		int serv_idx = atoi(args_get_from("server", &curr));
		int cli_idx  = atoi(args_get_from("client", &curr));

		assert(sinv_idx < BOOTER_MAX_SINV);
		sinv = &boot_sinvs[sinv_idx];
		sinv_idx++;	/* bump pointer allocation */

		crt_sinv_init(sinv, args_get_from("name", &curr), &boot_comps[serv_idx].comp, &boot_comps[cli_idx].comp,
			      strtoul(args_get_from("c_fn_addr", &curr), NULL, 10), strtoul(args_get_from("c_ucap_addr", &curr), NULL, 10),
			      strtoul(args_get_from("s_fn_addr", &curr), NULL, 10));

		printc("\t%s (%d->%d):\tclient_fn @ 0x%lx, client_ucap @ 0x%lx, server_fn @ 0x%lx\n",
		       sinv->name, sinv->client->id, sinv->server->id, sinv->c_fn_addr, sinv->c_ucap_addr, sinv->s_fn_addr);
	}

	for (j = 0 ; boot_comps[j].comp.name ; j++) {
		if (crt_comp_construct(&boot_comps[j].comp)) {
			printc("Error constructing the resource tables and image of component %s.\n",
			       boot_comps[j].comp.name);
		}
	}

	printc("DONE!\n");

	return;
}

static void
booter_init(void)
{
	struct cos_compinfo    *boot_info = boot_spd_compinfo_curr_get();
	struct comp_sched_info *bootsi    = boot_spd_comp_schedinfo_curr_get();

	cos_meminfo_init(&(boot_info->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
	bootsi->flags |= COMP_FLAG_SCHED;
}

void
cos_init(void)
{
	booter_init();
	comps_init();

	PRINTLOG(PRINT_DEBUG, "num cobjs: %d\n", num_cobj);
	/* assert(num_cobj <= MAX_NUM_SPDS); */
	/* boot_comp_capinfo_init(); */

	/* init_args = (struct component_init_str *)__cosrt_comp_info.cos_poly[3]; */
	/* boot_parse_init_args(); */
	/* init_args++; */

	/* boot_init_sched(); */
	/* boot_find_cobjs(h, num_cobj); */
	/* boot_bootcomp_init(); */
	/* boot_create_cap_system(); */
	/* boot_child_info_print(); */

	/* boot_done(); */
}
