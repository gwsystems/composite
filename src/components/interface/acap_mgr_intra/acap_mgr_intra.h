#ifndef ACAP_MGR_INTRA_H
#define ACAP_MGR_INTRA_H

#include <ck_ring_cos.h>
#include <../acap_mgr/acap_shared.h>

#define MAX_NESTED_PAR_LEVEL 8

int par_acap_lookup(int spdid, int n, int nest_level);
int par_create(int spdid, int n_request); // called by client
void *par_ring_lookup(int spdid, int n, int nest_level);
int par_get_wait_wakeup_acap(int spdid, int nest_level);

int par_srv_thd_num_lookup(int spdid); // called by server
int par_srv_acap_lookup(int spdid);
void *par_srv_ring_lookup(int spdid);
int par_parent_lookup(int spdid);

/* inv struct in ring buffer. */
struct intra_inv_data {
	void *data;
	void *fn;
	void *ret;
};

#ifndef __INTRA_RING_DEFINED
#define __INTRA_RING_DEFINED
CK_RING(intra_inv_data, intra_inv_ring);
#endif

#endif /* !ACAP_MGR_INTRA_H */
