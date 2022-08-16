#ifndef DCB_H
#define DCB_H

#include <cos_types.h>
#include <cos_kernel_api.h>

#define COS_DCB_PERPG_MAX (PAGE_SIZE / sizeof(struct cos_dcb_info))

#define COS_DCB_MAX_CAPS (MAX_NUM_THREADS / COS_DCB_PERPG_MAX + 1)

struct cos_dcbinfo_data {
	dcbcap_t dcbcaps[COS_DCB_MAX_CAPS];
	vaddr_t  dcbaddr[COS_DCB_MAX_CAPS];
	dcboff_t curr_cap_off;
	unsigned short curr_cap;

	struct cos_compinfo *ci;
} CACHE_ALIGNED;

int cos_dcb_test_111(void);
void cos_dcb_info_init(struct cos_dcbinfo_data *cdi, struct cos_compinfo *ci);
void cos_dcb_info_init_ext(struct cos_dcbinfo_data *cdi, struct cos_compinfo *ci, dcbcap_t initdcbcap, vaddr_t initdcbaddr, dcboff_t start_off);
dcbcap_t cos_dcb_info_alloc(struct cos_dcbinfo_data *cdi, dcboff_t *dcboff, vaddr_t *dcbaddr);

void cos_dcb_info_init_curr(void);
void cos_dcb_info_init_curr_ext(dcbcap_t initdcbcap, vaddr_t initdcbaddr, dcboff_t st_off);
dcbcap_t cos_dcb_info_alloc_curr(dcboff_t *dcboff, vaddr_t *dcbaddr);

#endif /* COS_DCB_H */
