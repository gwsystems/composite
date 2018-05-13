#ifndef CAPMGR_H
#define CAPMGR_H

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>

thdcap_t  capmgr_initthd_create(spdid_t child, thdid_t *tid);
thdcap_t  capmgr_initaep_create(spdid_t child, struct cos_aep_info *aep, int owntc, cos_aepkey_t key, asndcap_t *sndret);
thdcap_t  capmgr_thd_create(cos_thd_fn_t fn, void *data, thdid_t *tid);
thdcap_t  capmgr_aep_create(struct cos_aep_info *a, cos_aepthd_fn_t fn, void *data, int owntc, cos_aepkey_t key);
thdcap_t  capmgr_thd_create_ext(spdid_t child, thdclosure_index_t idx, thdid_t *tid);
thdcap_t  capmgr_aep_create_ext(spdid_t child, struct cos_aep_info *a, thdclosure_index_t idx, int owntc, cos_aepkey_t key, arcvcap_t *extrcv);
thdcap_t  capmgr_thd_retrieve(spdid_t child, thdid_t t);
thdcap_t  capmgr_thd_retrieve_next(spdid_t child, thdid_t *tid);
asndcap_t capmgr_asnd_create(spdid_t child, thdid_t t);
asndcap_t capmgr_asnd_key_create(cos_aepkey_t key);

int capmgr_hw_attach(hwid_t hwid, thdid_t tid);
int capmgr_hw_detach(hwid_t hwid);

#endif /* CAPMGR_H */
