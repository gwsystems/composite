/*
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 *
 * Copyright 2020, The George Washington University
 * Author: Gabe Parmer, gparmer@gwu.edu
 */

#ifndef CAPMGR_H
#define CAPMGR_H

/***
 * The capability manager is in charge of the controlling and
 * allocating resource tables (capability and page tables). It
 * controls creating all kernel resources, and the protocols for
 * delegation and revocation.
 */

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <cos_stubs.h>

typedef enum {
	CB_ADDR_SCB,
	CB_ADDR_DCB,
} ctrlblk_t;

void capmgr_set_tls(thdcap_t cap, void* tls_addr);

thdcap_t  capmgr_initthd_create(spdid_t child, thdid_t *tid, unsigned long *vas_id);
thdcap_t  COS_STUB_DECL(capmgr_initthd_create)(spdid_t child, thdid_t *tid, unsigned long *vas_id);

thdcap_t  capmgr_initaep_create(spdid_t child, struct cos_aep_info *aep, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, asndcap_t *sndret);
thdcap_t  COS_STUB_DECL(capmgr_initaep_create)(spdid_t child, struct cos_aep_info *aep, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, asndcap_t *sndret);

thdcap_t  capmgr_thd_create(cos_thd_fn_t fn, void *data, thdid_t *tid, struct cos_dcb_info **dcb);
thdcap_t  capmgr_aep_create(struct cos_aep_info *a, cos_aepthd_fn_t fn, void *data, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax);
arcvcap_t capmgr_rcv_alloc(cos_thd_fn_t fn, void *data, int flags, asndcap_t *asnd, thdcap_t *thdcap, thdid_t *tid);

thdcap_t capmgr_thd_create_thunk(thdclosure_index_t idx, thdid_t *tid, struct cos_dcb_info **dcb);
thdcap_t  COS_STUB_DECL(capmgr_thd_create_thunk)(thdclosure_index_t idx, thdid_t *tid, struct cos_dcb_info **dcb);

thdcap_t  capmgr_aep_create_thunk(struct cos_aep_info *a, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax);
thdcap_t  COS_STUB_DECL(capmgr_aep_create_thunk)(struct cos_aep_info *a, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax);

thdcap_t  capmgr_thd_create_ext(spdid_t child, thdclosure_index_t idx, thdid_t *tid, unsigned long *vas_id);
thdcap_t  COS_STUB_DECL(capmgr_thd_create_ext)(spdid_t child, thdclosure_index_t idx, thdid_t *tid, unsigned long *vas_id);

thdcap_t  capmgr_aep_create_ext(spdid_t child, struct cos_aep_info *a, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, arcvcap_t *extrcv);
thdcap_t  COS_STUB_DECL(capmgr_aep_create_ext)(spdid_t child, struct cos_aep_info *a, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, arcvcap_t *extrcv);

arcvcap_t capmgr_rcv_create(thdclosure_index_t idx, int flags, asndcap_t *asnd, thdcap_t *thdcap, thdid_t *tid);
arcvcap_t COS_STUB_DECL(capmgr_rcv_create)(thdclosure_index_t idx, int flags, asndcap_t *asnd, thdcap_t *thdcap, thdid_t *tid);

asndcap_t capmgr_asnd_create(spdid_t child, thdid_t t);
asndcap_t COS_STUB_DECL(capmgr_asnd_create)(spdid_t child, thdid_t t);

asndcap_t capmgr_asnd_rcv_create(arcvcap_t rcv);
asndcap_t COS_STUB_DECL(capmgr_asnd_rcv_create)(arcvcap_t rcv);

vaddr_t capmgr_scb_map_ro(void);
vaddr_t COS_STUB_DECL(capmgr_scb_map_ro)(void);

asndcap_t capmgr_asnd_key_create(cos_channelkey_t key);
asndcap_t COS_STUB_DECL(capmgr_asnd_key_create)(cos_channelkey_t key);

vaddr_t capmgr_sched_initdcb_get(void);
vaddr_t COS_STUB_DECL(capmgr_sched_initdcb_get)(void);

thdid_t capmgr_retrieve_dcbinfo(thdid_t tid, struct cos_dcb_info **dcb);
thdid_t COS_STUB_DECL(capmgr_retrieve_dcbinfo)(thdid_t tid, struct cos_dcb_info **dcb);

#endif /* CAPMGR_H */
