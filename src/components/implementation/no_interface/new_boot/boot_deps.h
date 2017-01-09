#include <print.h>

#undef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0;} } while(0)

#include <cos_alloc.h>
#include <cobj_format.h>
#include <cos_types.h>

/* Assembly function for sinv from new component */
extern void *__inv_test_entry(int a, int b, int c);

struct cobj_header *hs[MAX_NUM_SPDS+1];

/* The booter uses this to keep track of each comp */
struct comp_cap_info {
	struct cos_compinfo *compinfo;
	vaddr_t addr_start;
	vaddr_t vaddr_mapped_in_booter;
	vaddr_t upcall_entry;
}new_comp_cap_info[MAX_NUM_SPDS+1];

struct cos_compinfo boot_info;
struct cos_compinfo new_compinfo[MAX_NUM_SPDS+1];

thdcap_t schedule[MAX_NUM_SPDS+1];
unsigned int sched_cur;

/* Macro for sinv back to booter from new component */
enum {
	BOOT_SINV_CAP = round_up_to_pow2(BOOT_CAPTBL_FREE + CAP32B_IDSZ, CAPMAX_ENTRY_SZ)
};

static int
boot_comp_map_memory(struct cobj_header *h, spdid_t spdid, pgtblcap_t pt)
{
	int i, j;
	int flag;
	vaddr_t dest_daddr, prev_map = 0;
	int tot = 0, n_pte = 1;	
	struct cobj_sect *sect = cobj_sect_get(h, 0);
	
	/* Expand Page table, could do this faster */
	for (j = 0 ; j < (int)h->nsect ; j++) {
		tot += cobj_sect_size(h, j);
	}
	
	if (tot > SERVICE_SIZE) {
		n_pte = tot / SERVICE_SIZE;
		if (tot % SERVICE_SIZE) n_pte++;
	}	

	for (j = 0 ; j < n_pte ; j++) {
		if (!cos_pgtbl_intern_alloc(&boot_info, pt, sect->vaddr, SERVICE_SIZE)) BUG();
	}

	/* We'll map the component into booter's heap. */
	new_comp_cap_info[spdid].vaddr_mapped_in_booter = (vaddr_t)cos_get_heap_ptr();
	
	for (i = 0 ; i < h->nsect ; i++) {
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
			vaddr_t addr = cos_page_bump_alloc(&boot_info);
			assert(addr);
			
			if (cos_mem_alias_at(new_comp_cap_info[spdid].compinfo, dest_daddr, &boot_info, addr)) BUG();
			prev_map = dest_daddr;
			dest_daddr += PAGE_SIZE;
			left       -= PAGE_SIZE;
		}
	}

	return 0;
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
boot_newcomp_create(int spdid, captblcap_t ct, pgtblcap_t pt)
{
		compcap_t cc;
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
		while (schedule[i] != NULL) i++;
		schedule[i] = main_thd;
}

static void
boot_init_sched(void)
{
	int i;
	
	for (i = 0 ; i < MAX_NUM_SPDS ; i++) schedule[i] = NULL;
	sched_cur = 0;
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

void
boot_thd_done(void)
{
	printc("\nWelcome back to the booter!\n");
	sched_cur++;

	if (schedule[sched_cur] != NULL) {
		printc("Initializing comp: %d\n", sched_cur);
	       	cos_thd_switch(schedule[sched_cur]);
	} else {
	       	printc("Done Initializing\n");
	}

	printc("What to do now... hm..\n");
}

