#undef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0;} } while(0)

#include <cos_alloc.h>
#include <cobj_format.h>
#include <cos_types.h>
#include <llprint.h>

/* Assembly function for sinv from new component */
extern void *__inv_test_entry(int a, int b, int c);

struct cobj_header *hs[MAX_NUM_SPDS+1];

/* The booter uses this to keep track of each comp */
struct comp_cap_info {
	struct cos_compinfo *compinfo;
	vaddr_t addr_start;
	vaddr_t vaddr_mapped_in_booter;
	vaddr_t upcall_entry;
} new_comp_cap_info[MAX_NUM_SPDS+1];

struct cos_compinfo boot_info;
struct cos_compinfo new_compinfo[MAX_NUM_SPDS+1];

thdcap_t schedule[MAX_NUM_SPDS+1];
volatile unsigned int sched_cur;

/* Macro for sinv back to booter from new component */
enum {
	BOOT_SINV_CAP = round_up_to_pow2(BOOT_CAPTBL_FREE + CAP32B_IDSZ, CAPMAX_ENTRY_SZ)
};

static vaddr_t
boot_deps_map_sect(int spdid, vaddr_t dest_daddr)
{
	vaddr_t addr = (vaddr_t) cos_page_bump_alloc(&boot_info);
	assert(addr);

	if (cos_mem_alias_at(new_comp_cap_info[spdid].compinfo, dest_daddr, &boot_info, addr)) BUG();

	return addr;
}

static void
boot_comp_pgtbl_expand(int n_pte, pgtblcap_t pt, vaddr_t vaddr, struct cobj_header *h)
{
	int i;
	int tot = 0;
	/* Expand Page table, could do this faster */
	for (i = 0 ; i < (int)h->nsect ; i++) {
		tot += cobj_sect_size(h, i);
	}

	if (tot > SERVICE_SIZE) {
		n_pte = tot / SERVICE_SIZE;
		if (tot % SERVICE_SIZE) n_pte++;
	}

	for (i = 0 ; i < n_pte ; i++) {
		if (!cos_pgtbl_intern_alloc(&boot_info, pt, vaddr, SERVICE_SIZE)) BUG();
	}
}

/* Initialize just the captblcap and pgtblcap, due to hack for upcall_fn addr */
static void
boot_compinfo_init(int spdid, captblcap_t *ct, pgtblcap_t *pt, u32_t vaddr)
{
	*ct = cos_captbl_alloc(&boot_info);
	assert(*ct);
	*pt = cos_pgtbl_alloc(&boot_info);
	assert(*pt);

	new_comp_cap_info[spdid].compinfo = &new_compinfo[spdid];
	cos_compinfo_init(new_comp_cap_info[spdid].compinfo, *pt, *ct, 0,
				  (vaddr_t)vaddr, 4, &boot_info);
}

static void
boot_newcomp_create(int spdid, struct cos_compinfo *comp_info)
{
	compcap_t   cc;
	captblcap_t ct = new_comp_cap_info[spdid].compinfo->captbl_cap;
	pgtblcap_t  pt = new_comp_cap_info[spdid].compinfo->pgtbl_cap;
	sinvcap_t sinv;
	thdcap_t main_thd;
	int i = 0;

	cc = cos_comp_alloc(&boot_info, ct, pt, (vaddr_t)new_comp_cap_info[spdid].upcall_entry);
	assert(cc);
	new_comp_cap_info[spdid].compinfo->comp_cap = cc;

	/* Create sinv capability from Userspace to Booter components */
	sinv = cos_sinv_alloc(&boot_info, boot_info.comp_cap, (vaddr_t)__inv_test_entry);
	assert(sinv > 0);

	/* Copy sinv into new comp's capability table at a known location (BOOT_SINV_CAP) */
	cos_cap_cpy_at(new_comp_cap_info[spdid].compinfo, BOOT_SINV_CAP, &boot_info, sinv);

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
	printc("Booter: done creating system.\n");
	cos_thd_switch(schedule[sched_cur]);
}

/* Run after a componenet is done init execution, via sinv() into booter */
void
boot_thd_done(void)
{
	sched_cur++;

	if (schedule[sched_cur] != 0) {
		printc("Initializing comp: %d\n", sched_cur);
	       	cos_thd_switch(schedule[sched_cur]);
	} else {
	       	printc("Done Initializing\n");
	}
}
