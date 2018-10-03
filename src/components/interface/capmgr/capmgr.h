/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#ifndef CAPMGR_H
#define CAPMGR_H

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>

thdcap_t  capmgr_initthd_create(spdid_t child, thdid_t *tid);
thdcap_t  capmgr_initaep_create(spdid_t child, struct cos_aep_info *aep, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, asndcap_t *sndret);
thdcap_t  capmgr_thd_create(cos_thd_fn_t fn, void *data, thdid_t *tid);
thdcap_t  capmgr_aep_create(struct cos_aep_info *a, cos_aepthd_fn_t fn, void *data, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax);
thdcap_t  capmgr_thd_create_ext(spdid_t child, thdclosure_index_t idx, thdid_t *tid);
thdcap_t  capmgr_aep_create_ext(spdid_t child, struct cos_aep_info *a, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, arcvcap_t *extrcv);
thdcap_t  capmgr_thd_retrieve(spdid_t child, thdid_t t, thdid_t *inittid);
thdcap_t  capmgr_thd_retrieve_next(spdid_t child, thdid_t *tid);
arcvcap_t capmgr_rcv_create(spdid_t child, thdid_t tid, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax);
asndcap_t capmgr_asnd_create(spdid_t child, thdid_t t);
asndcap_t capmgr_asnd_rcv_create(arcvcap_t rcv);
asndcap_t capmgr_asnd_rcv_create_raw(arcvcap_t rcv); /* creates raw asnd cap instead of sinv cap for cross-core comm. */
asndcap_t capmgr_asnd_key_create(cos_channelkey_t key);
asndcap_t capmgr_asnd_key_create_raw(cos_channelkey_t key);

int capmgr_hw_attach(hwid_t hwid, thdid_t tid);
int capmgr_hw_periodic_attach(hwid_t hwid, thdid_t tid, unsigned int period_us);
int capmgr_hw_detach(hwid_t hwid);

/*
 * @type: 1 for capmgr counters, 0 for kernel counters
 */
int capmgr_core_ipi_counters_get(cpuid_t core, unsigned int type, unsigned int *sndctr, unsigned int *rcvctr);

#endif /* CAPMGR_H */
