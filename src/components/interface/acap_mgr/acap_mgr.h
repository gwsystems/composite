#ifndef ACAP_MGR_H
#define ACAP_MGR_H

int acap_cli_lookup(int spdid, int s_cap);
void *acap_cli_lookup_ring(int spdid, int s_cap);
void *acap_srv_lookup_ring(int spdid);
int ainv_init(int spdid);
int acap_srv_lookup(int spdid);
int acap_srv_ncaps(int spdid);
void *acap_srv_fn_mapping(int spdid, int cap);

#include <ck_ring_cos.h>

struct inv_data {
	int cap;
	int params[4];
	int return_idx;
}; /* Q: do we want this to use a single cache line? */

#define CK_RING_CONTINUOUS

#ifndef __RING_DEFINED
#define __RING_DEFINED
CK_RING(inv_data, inv_ring);
#endif

struct shared_struct {
	/* shared_page structures here! */
	int *server_active;
	CK_RING_INSTANCE(inv_ring) *ring;
	void *ret_map; // TODO: add this!
};

#define SERVER_ACTIVE(curr)        (*curr->server_active == 1)
#define SET_SERVER_ACTIVE(curr)    (*curr->server_active = 1)
#define CLEAR_SERVER_ACTIVE(curr)  (*curr->server_active = 0)

static void init_shared_page(struct shared_struct *curr, void *page) {
	/*The ring starts from the second cache line of the
	 * page. (First cache line is used for the server thread
	 * active flag)*/
	curr->server_active = (int *)page;
	/* ring initialized by acap mgr. see comments in
	 * alloc_share_page in acap_mgr. */
	curr->ring = (CK_RING_INSTANCE(inv_ring) *) (page + CACHE_LINE);
	/* return bitmap uses the second half of the page. */
	curr->ret_map =  page + PAGE_SIZE / 2;
}

#endif /* !ACAP_MGR_H */
