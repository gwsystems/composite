#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <llbooter_inv.h>
#include <tlsmgr.h>
#include <consts.h>

struct cos_compinfo *tlsmgr_cinfo;

/*
 * Notes:
 * Upon tls manager component initialization, every component gets x number of
 *   pages placed starting at 0x7... where x is the number of pages required to
 *   fit y amount of memory for the max number of threads in the system.
 *
 * Upon thread creation, every thread will have its tls region set to its specific
 *   vaddr corresponding to 0x7..., that way, no matter where the thread executes, it will
 *   have its own vaddr chunk dedicated for it with backing pages that differ per component
 *
 * To use cos_thd_mod to set the tls region, one must have the thread within one's captbl,
 *   because of this, return the vaddr to the component and at the end of the function
 *   call "sinv call", have the calling component do it themselves
 */

unsigned int tlsmgr_token;

void *
tlsmgr_alloc(unsigned int dst_tid)
{
	assert(dst_tid < MAX_NUM_THREADS);
	return TLS_BASE_ADDR + (void *)(TLS_AREA_SIZE * dst_tid);
}

void
__set_tls_regions(void)
{
	capid_t cap_index;
	size_t num_comps = -1;
	size_t i, j, offset;
	vaddr_t src_pg;
	int ret;

	/* Get the number of page tables available to use to copy */
	num_comps = (int)cos_sinv(BOOT_CAPTBL_SINV_CAP, BOOT_HYP_NUM_COMPS, 0, 0, 0);
	assert(num_comps);
	printc("We need to transfer %d pgtbls and write %lu number of pages into them...\n",
		num_comps-1, TLS_NUM_PAGES);

	for (i = 1 ; i <= num_comps ; i++) {
		/* Already have access to my own page table */
		if (i == tlsmgr_token) continue;

		/* Get page table */
		cap_index = (unsigned int)cos_hypervisor_hypercall(BOOT_HYP_PGTBL_CAP,
				(void *)tlsmgr_token, (void *)i, (void *)tlsmgr_cinfo);
		assert(cap_index > 0);

		/* Expand the 2nd level pte within this component's page table at our new range */
		struct cos_compinfo tmp;
		tmp.pgtbl_cap = cap_index;
		tmp.memsrc    = tlsmgr_cinfo;
		ret = (int)cos_pgtbl_intern_alloc(&tmp, cap_index, TLS_BASE_ADDR,
					     PAGE_SIZE * TLS_NUM_PAGES);
		assert(ret);

		/* Place desired number of pages at this new range */
		offset = 0;
		for (j = 0 ; j < TLS_NUM_PAGES ; j++) {
			src_pg = (vaddr_t)cos_page_bump_alloc(tlsmgr_cinfo);
			assert(src_pg);
			ret = cos_mem_alias_at(&tmp, TLS_BASE_ADDR + offset, tlsmgr_cinfo, src_pg);
			assert(!ret);
			offset += PAGE_SIZE;
		}
	}

	printc("Done\n");

}


void
cos_init(void)
{
	struct cos_defcompinfo *dci;
	struct cos_config_info_t *my_info;

	printc("Initializing TLS Manager Component\n");
	printc("cos_component_information spdid: %ld\n", cos_comp_info.cos_this_spd_id);
	tlsmgr_token = cos_comp_info.cos_this_spd_id;

	dci = cos_defcompinfo_curr_get();
	assert(dci);
	tlsmgr_cinfo = cos_compinfo_get(dci);
	assert(tlsmgr_cinfo);

	cos_defcompinfo_init();
	cos_meminfo_init(&(tlsmgr_cinfo->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ,
			BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(tlsmgr_cinfo, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT,
			BOOT_CAPTBL_SELF_COMP, (vaddr_t)cos_get_heap_ptr(),
			BOOT_CAPTBL_FREE, tlsmgr_cinfo);

	/* Get access to the page tables from the booter of the components we will be servicing */
	__set_tls_regions();

	cos_hypervisor_hypercall(BOOT_HYP_INIT_DONE, 0, 0, 0);
}
