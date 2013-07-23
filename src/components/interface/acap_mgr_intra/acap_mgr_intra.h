#ifndef ACAP_MGR_INTRA_H
#define ACAP_MGR_INTRA_H

#include <../acap_mgr/acap_shared.h>

int ainv_intra_create(int spdid, int n_request, void *fn); // called by client
int ainv_intra_lookup(int spdid, int n_request, void *fn); // called by client
void *ainv_intra_lookup_ring(int spdid, int n_request, void *fn); // called by client
int acap_srv_lookup_thd_num(int spdid); // called by server
void *acap_srv_lookup_ring_intra(int spdid);
int acap_srv_lookup_intra(int spdid);
int ainv_intra_wait_acap_create(int spdid);
int ainv_intra_wakeup_acap_lookup(int spdid);

#include <ck_ring_cos.h>

struct intra_inv_data {
	void *data;
	void *fn;
	void *ret;
};

#define CK_RING_CONTINUOUS

#ifndef __INTRA_RING_DEFINED
#define __INTRA_RING_DEFINED
CK_RING(intra_inv_data, intra_inv_ring);
#endif

struct intra_shared_struct {
	int *server_active;
	CK_RING_INSTANCE(intra_inv_ring) *ring;
};

#define SERVER_ACTIVE(curr)        (*curr->server_active == 1)
#define SET_SERVER_ACTIVE(curr)    (*curr->server_active = 1)
#define CLEAR_SERVER_ACTIVE(curr)  (*curr->server_active = 0)

struct __cos_ainv_srv_thd_intra {
	int acap;
	volatile int stop;

	void *shared_page;
	struct intra_shared_struct intra_shared_struct;
} CACHE_ALIGNED;

static void init_intra_shared_page(struct intra_shared_struct *curr, void *page) {
	/*The ring starts from the second cache line of the
	 * page. (First cache line is used for the server thread
	 * active flag)*/
	curr->server_active = (int *)page;
	/* ring initialized by acap mgr. see comments in
	 * alloc_share_page in acap_mgr. */
	curr->ring = (CK_RING_INSTANCE(intra_inv_ring) *) (page + CACHE_LINE);
}

#endif /* !ACAP_MGR_INTRA_H */
