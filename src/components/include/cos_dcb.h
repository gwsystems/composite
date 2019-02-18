#ifndef COS_DCB_H
#define COS_DCB_H

#include <cos_types.h>

#define COS_DCB_PERPG_MAX (PAGE_SIZE / sizeof(struct cos_dcb_info))

void cos_dcb_info_init(void);
struct cos_dcb_info *cos_dcb_info_assign(void);

#endif /* COS_DCB_H */
