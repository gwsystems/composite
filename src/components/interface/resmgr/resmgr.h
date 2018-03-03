#ifndef RESMGR_H
#define RESMGR_H

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>

thdcap_t  resmgr_initthd_create(spdid_t child);
thdcap_t  resmgr_initaep_create(spdid_t child, struct cos_aep_info *aep, int owntc, asndcap_t *sndret);
thdcap_t  resmgr_thd_create(cos_thd_fn_t fn, void *data);
thdcap_t  resmgr_aep_create(struct cos_aep_info *a, cos_aepthd_fn_t fn, void *data, int owntc);
thdcap_t  resmgr_ext_thd_create(spdid_t child, int idx);
thdcap_t  resmgr_ext_aep_create(spdid_t child, struct cos_aep_info *a, int idx, int owntc, arcvcap_t *extrcv);
thdcap_t  resmgr_thd_retrieve(spdid_t child, thdid_t t);
thdid_t   resmgr_thd_retrieve_next(spdid_t child, thdcap_t *t);
asndcap_t resmgr_asnd_create(spdid_t child, thdid_t t);

#endif /* RESMGR_H */
