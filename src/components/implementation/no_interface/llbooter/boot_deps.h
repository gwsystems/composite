#include <cobj_format.h>
#include <cos_alloc.h>
#include <cos_debug.h>
#include <cos_types.h>
#include <llprint.h>

#define UNDEF_SYMBS 64

/* Assembly function for sinv from new component */
extern void *__inv_test_entry(int a, int b, int c);

struct cobj_header *hs[MAX_NUM_SPDS + 1];

/* The booter uses this to keep track of each comp */
struct comp_cap_info {
	struct cos_compinfo *compinfo;
	struct usr_inv_cap   ST_user_caps[UNDEF_SYMBS];
	vaddr_t              vaddr_user_caps; // vaddr of user caps table in comp
	vaddr_t              addr_start;
	vaddr_t              vaddr_mapped_in_booter;
	vaddr_t              upcall_entry;
} new_comp_cap_info[MAX_NUM_SPDS + 1];

struct cos_compinfo boot_info;
struct cos_compinfo new_compinfo[MAX_NUM_SPDS + 1];

int                      schedule[MAX_NUM_SPDS + 1];
volatile size_t          sched_cur;

static vaddr_t
boot_deps_map_sect(spdid_t spdid, vaddr_t dest_daddr)
{
	vaddr_t addr = (vaddr_t)cos_page_bump_alloc(&boot_info);
	assert(addr);

	if (cos_mem_alias_at(new_comp_cap_info[spdid].compinfo, dest_daddr, &boot_info, addr)) BUG();

	return addr;
}

static void
boot_comp_pgtbl_expand(size_t n_pte, pgtblcap_t pt, vaddr_t vaddr, struct cobj_header *h)
{
	size_t i;
	int tot = 0;
	/* Expand Page table, could do this faster */
	for (i = 0; i < (size_t)h->nsect; i++) {
		tot += cobj_sect_size(h, i);
	}

	if (tot > SERVICE_SIZE) {
		n_pte = tot / SERVICE_SIZE;
		if (tot % SERVICE_SIZE) n_pte++;
	}

	for (i = 0; i < n_pte; i++) {
		if (!cos_pgtbl_intern_alloc(&boot_info, pt, vaddr, SERVICE_SIZE)) BUG();
		/* Increment vaddr incase we loop again */
		vaddr += SERVICE_SIZE;
	}
}

/* Initialize just the captblcap and pgtblcap, due to hack for upcall_fn addr */
static void
boot_compinfo_init(spdid_t spdid, captblcap_t *ct, pgtblcap_t *pt, u32_t vaddr)
{
	*ct = cos_captbl_alloc(&boot_info);
	assert(*ct);
	*pt = cos_pgtbl_alloc(&boot_info);
	assert(*pt);

	new_comp_cap_info[spdid].compinfo = &new_compinfo[spdid];
	cos_compinfo_init(new_comp_cap_info[spdid].compinfo, *pt, *ct, 0, (vaddr_t)vaddr, 4, &boot_info);
}

static void
boot_newcomp_sinv_alloc(spdid_t spdid)
{
	sinvcap_t sinv;
	int i = 0;
	int intr_spdid;
	void *user_cap_vaddr;
	struct cos_compinfo *interface_compinfo;
	struct cos_compinfo *newcomp_compinfo = new_comp_cap_info[spdid].compinfo;

	/*
	 * Loop through all undefined symbs
	 */
	for (i = 0; i < UNDEF_SYMBS; i++) {
		if ( new_comp_cap_info[spdid].ST_user_caps[i].service_entry_inst > 0) {

			intr_spdid = new_comp_cap_info[spdid].ST_user_caps[i].invocation_count;
			interface_compinfo = new_comp_cap_info[intr_spdid].compinfo;
			user_cap_vaddr = (void *) (new_comp_cap_info[spdid].vaddr_mapped_in_booter + (new_comp_cap_info[spdid].vaddr_user_caps - new_comp_cap_info[spdid].addr_start) + (sizeof(struct usr_inv_cap) * i));

			/* Create sinv capability from client to server */
			sinv = cos_sinv_alloc(newcomp_compinfo, interface_compinfo->comp_cap, (vaddr_t)new_comp_cap_info[spdid].ST_user_caps[i].service_entry_inst);
			assert(sinv > 0);

			new_comp_cap_info[spdid].ST_user_caps[i].cap_no = sinv;

			/* Now that we have the sinv allocated, we can copy in the symb user cap to correct index */
			memcpy(user_cap_vaddr, &new_comp_cap_info[spdid].ST_user_caps[i], sizeof(struct usr_inv_cap));
		}
	}
}

static void
boot_newcomp_create(spdid_t spdid, struct cos_compinfo *comp_info)
{
	compcap_t   cc;
	captblcap_t ct = new_comp_cap_info[spdid].compinfo->captbl_cap;
	pgtblcap_t  pt = new_comp_cap_info[spdid].compinfo->pgtbl_cap;
	sinvcap_t   sinv;
	thdcap_t    main_thd;
	int         i = 0;

	cc = cos_comp_alloc(&boot_info, ct, pt, (vaddr_t)new_comp_cap_info[spdid].upcall_entry);
	assert(cc);
	new_comp_cap_info[spdid].compinfo->comp_cap = cc;

	/* Create sinv capability from Userspace to Booter components */
	sinv = cos_sinv_alloc(&boot_info, boot_info.comp_cap, (vaddr_t)__inv_test_entry);
	assert(sinv > 0);

	cos_cap_cpy_at(new_comp_cap_info[spdid].compinfo, BOOT_CAPTBL_SINV_CAP, &boot_info, sinv);

	boot_newcomp_sinv_alloc(spdid);

	main_thd = cos_initthd_alloc(&boot_info, cc);
	assert(main_thd);

	/* Add created component to "scheduling" array */
	while (schedule[i] != 0) i++;
	schedule[i] = main_thd;
}

static void
boot_bootcomp_init(void)
{
	cos_meminfo_init(&boot_info.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);

	cos_compinfo_init(&boot_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
	                  (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &boot_info);
}

static void
boot_done(void)
{
	printc("Booter: done creating system.\n\n");
	cos_thd_switch(schedule[sched_cur]);
}

/* Run after a componenet is done init execution, via sinv() into booter */
void
boot_thd_done(void)
{
	sched_cur++;

	if (schedule[sched_cur] != 0) {
		cos_thd_switch(schedule[sched_cur]);
	} else {
		printc("Done Initializing\n");
	}
}
