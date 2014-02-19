#ifndef PAR_MGR_H
#define PAR_MGR_H

#include <ck_ring_cos.h>

int acap_cli_lookup(int spdid, int s_cap, int thd_init_idx);
void *acap_cli_lookup_ring(int spdid, int s_cap);
void *acap_srv_lookup_ring(int spdid);
int acap_srv_lookup(int spdid);
int acap_srv_ncaps(int spdid);
void *acap_srv_fn_mapping(int spdid, int cap);

#define MAX_OMP_NESTED_PAR_LEVEL 8 

int par_acap_lookup(int spdid, int n, int nest_level, int thd_init_idx);
int par_create(int spdid, int n_request); // called by client
void *par_ring_lookup(int spdid, int n, int nest_level);
int par_acap_get_barrier(int spdid, int nest_level);

int par_srv_thd_num_lookup(int spdid); // called by server
int par_srv_acap_lookup(int spdid);
void *par_srv_ring_lookup(int spdid);
int par_parent_lookup(int spdid);

#ifndef __RING_DEFINED
#define __RING_DEFINED
struct inv_data {
	int cap;
	int params[4];
//	int return_idx;
};

CK_RING(inv_data, inv_ring);
#endif

#ifndef __INTRA_RING_DEFINED
#define __INTRA_RING_DEFINED
/* inv struct in ring buffer. */
struct __intra_inv_data {
	void *data;
	void *fn;
	void *ret;
};

CK_RING(__intra_inv_data, intra_inv_ring);
#endif

struct shared_struct {
	/* shared_page structures here! */
	int *server_active;
	CK_RING_INSTANCE(inv_ring) *ring;
	void *ret_map; // TODO: add this!
};

struct cap_info {
	int acap;
	int static_inv;
	void *shared_page;
	struct shared_struct shared_struct;
};

struct ainv_info {
	int thdid; // owner thread
	struct cap_info *cap[MAX_STATIC_CAP]; // static cap to acap mapping
} CACHE_ALIGNED;

struct __cos_ainv_srv_thd {
	int acap;
	int cli_ncaps;
	vaddr_t *fn_mapping;
	volatile int stop;

	void *shared_page;
	struct shared_struct shared_struct;
} CACHE_ALIGNED;

#define SERVER_ACTIVE(curr)        (*curr->server_active == 1)
#define SET_SERVER_ACTIVE(curr)    (*curr->server_active = 1)
#define CLEAR_SERVER_ACTIVE(curr)  (*curr->server_active = 0)

static void init_shared_page(struct shared_struct *curr, void *page) {
	/*The ring starts from the second cache line of the
	 * page. (First cache line is used for the server thread
	 * active flag)*/
	curr->server_active = (int *)page;
	/* ring initialized by acap mgr. see comments in
	 * alloc_share_page in par_mgr. */
	curr->ring = (CK_RING_INSTANCE(inv_ring) *) (page + CACHE_LINE);
	curr->ret_map =  page + PAGE_SIZE / 2;
}

static inline int 
exec_fn(int (*fn)(), int nparams, int *params)
{
	int ret;

	assert(fn);

	switch (nparams)
	{		
	case 0:
		ret = fn();
		break;
	case 1:
		ret = fn(params[0]);
		break;
	case 2:
		ret = fn(params[0], params[1]);
		break;
	case 3:
		ret = fn(params[0], params[1], params[2]);
		break;
	case 4:
		ret = fn(params[0], params[1], params[2], params[3]);
		break;
	}
	
	return ret;
}

#endif /* !PAR_MGR_H */
