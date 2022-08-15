#include <cos_component.h>
#include <dcb.h>
#include <cos_defkernel_api.h>

static struct cos_dcbinfo_data _cos_dcbinfo[NUM_CPU];

int
cos_dcb_test_111(void) {
	assert(0);
	return 1;
}
	
void
cos_dcb_info_init_ext(struct cos_dcbinfo_data *cdi, struct cos_compinfo *ci, 
		      dcbcap_t initdcbcap, vaddr_t initdcbaddr, dcboff_t start_off)
{
	memset(cdi, 0, sizeof(struct cos_dcbinfo_data));

	cdi->dcbcaps[0]   = initdcbcap;
	cdi->dcbaddr[0]   = initdcbaddr;
	cdi->curr_cap_off = start_off;
	cdi->curr_cap     = 0;
}

void
cos_dcb_info_init(struct cos_dcbinfo_data *cdi, struct cos_compinfo *ci)
{
	if (cos_spd_id() == 0) {
		cos_dcb_info_init_ext(cdi, ci, LLBOOT_CAPTBL_CPU_INITDCB, 
				      (vaddr_t)cos_init_dcb_get(), 1);
	} else {
		cos_dcb_info_init_ext(cdi, ci, 0, 0, 0);
	}
}

void
cos_dcb_info_init_curr(void)
{
	cos_dcb_info_init_curr_ext(0, 0, 0);
}

void
cos_dcb_info_init_curr_ext(dcbcap_t initdcbcap, vaddr_t initdcbaddr, dcboff_t st_off)
{
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());

	if (initdcbcap == 0 && initdcbaddr == 0) {

		if (cos_spd_id() == 0) {
			cos_dcb_info_init_ext(&_cos_dcbinfo[cos_cpuid()], ci, 
					      LLBOOT_CAPTBL_CPU_INITDCB, (vaddr_t)cos_init_dcb_get(), 1);

			return;
		} else {
			initdcbaddr = cos_page_bump_intern_valloc(ci, PAGE_SIZE);
			assert(initdcbaddr);
			initdcbcap  = cos_dcb_alloc(ci, ci->pgtbl_cap, initdcbaddr);
			assert(initdcbcap);
			st_off = 0;
		}
	}
	cos_dcb_info_init_ext(&_cos_dcbinfo[cos_cpuid()], ci, initdcbcap, initdcbaddr, st_off);
}

dcbcap_t
cos_dcb_info_alloc_curr(dcboff_t *dcboff, vaddr_t *dcbaddr)
{
	return cos_dcb_info_alloc(&_cos_dcbinfo[cos_cpuid()], dcboff, dcbaddr);
}

dcbcap_t
cos_dcb_info_alloc(struct cos_dcbinfo_data *cdi, dcboff_t *dcboff, vaddr_t *dcbaddr)
{
	if (unlikely(cdi->dcbcaps[cdi->curr_cap] == 0)) {
		*dcboff = 0;
		*dcbaddr = 0;

		return 0;
	}
	if (cdi->curr_cap_off >= COS_DCB_PERPG_MAX) {
		int ret;
		unsigned short curr_off = cdi->curr_cap;

		assert(curr_off + 1 < (unsigned short)COS_DCB_MAX_CAPS && cdi->dcbcaps[curr_off + 1] == 0);

		cdi->dcbaddr[curr_off + 1] = cos_page_bump_intern_valloc(cdi->ci, PAGE_SIZE);
		assert(cdi->dcbaddr[curr_off + 1]);
		cdi->dcbcaps[curr_off + 1] = cos_dcb_alloc(cos_compinfo_get(cos_defcompinfo_curr_get()), 
							   cdi->ci->pgtbl_cap, cdi->dcbaddr[curr_off + 1]);

		assert(cdi->dcbcaps[curr_off + 1]);
		ret = ps_cas((unsigned long *)&cdi->curr_cap, curr_off, curr_off + 1);
		assert(ret);
		ret = ps_cas((unsigned long *)&cdi->curr_cap_off, cdi->curr_cap_off, 0);
		assert(ret);
	}

	*dcboff  = ps_faa((unsigned long *)&cdi->curr_cap_off, 1);
	*dcbaddr = cdi->dcbaddr[cdi->curr_cap] + (sizeof(struct cos_dcb_info) * (*dcboff));

	return cdi->dcbcaps[cdi->curr_cap];
}
