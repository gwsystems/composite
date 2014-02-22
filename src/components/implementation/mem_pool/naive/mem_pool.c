#include <cos_component.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_list.h>
#include <cinfo.h>
#include <sched.h>
#include <mem_mgr_large.h>

#include <valloc.h>
#include <mem_pool.h>

#include <cos_vect.h>
#include <cos_synchronization.h>
cos_lock_t pool_l;
#define TAKE()    do { if (unlikely(lock_take(&pool_l) != 0)) BUG(); }   while(0)
#define RELEASE() do { if (unlikely(lock_release(&pool_l) != 0)) BUG() } while(0)
#define LOCK_INIT()    lock_static_init(&pool_l);

COS_VECT_CREATE_STATIC(page_descs_1);
COS_VECT_CREATE_STATIC(page_descs_2);
cos_vect_t * page_descs[NUM_TMEM_MGR] = {&page_descs_1, &page_descs_2};

struct tmem_mgr {
	cos_vect_t * mgr_page_descs;
	spdid_t spdid;
	u32_t thdid;
	u32_t mgr_allocated, mgr_desired;
	u32_t glb_blked;
	u32_t evt_thd_waiting;
	struct tmem_mgr *next_mgr;
};

struct page_item {
	struct page_item *free_next;
	void *l_addr, *mgr_addr;
	int size;
};

struct tmem_mgr *all_tmem_mgr[MAX_NUM_SPDS];
// Holds all currently free pages
struct page_item *free_page_list;
struct page_item all_page_list[MAX_NUM_MEM];

int unregistered_mgrs = NUM_TMEM_MGR;

struct tmem_mgr *tmem_mgr_list = NULL;

#define ADDO_MGR_LIST(mgr)				\
	mgr->next_mgr = tmem_mgr_list;			\
	tmem_mgr_list = mgr;


static inline void freelist_add(struct page_item *page)
{
	page->free_next = free_page_list;
	free_page_list = page;
}

int mempool_put_mem(spdid_t d_spdid, void* mgr_addr)
{
	struct page_item *page;
	struct tmem_mgr *mgr;

	TAKE();
	mgr = all_tmem_mgr[d_spdid];
	if (unlikely(!mgr)) goto err;

	page = (struct page_item *)cos_vect_lookup(mgr->mgr_page_descs, (u32_t)mgr_addr>>PAGE_ORDER);
	if (unlikely(!page)) goto err;

	cos_vect_del(mgr->mgr_page_descs, (u32_t)mgr_addr>>PAGE_ORDER);
	freelist_add(page);
	mman_revoke_page(cos_spd_id(), (vaddr_t)(page->l_addr), 0); 
	valloc_free(cos_spd_id(), d_spdid, mgr_addr, 1);
	mgr->mgr_allocated--;

	mgr = tmem_mgr_list;
	/* wake up all event threads if necessary! */
	while (mgr) {
		if (mgr->glb_blked && mgr->mgr_desired > mgr->mgr_allocated && mgr->evt_thd_waiting) {
			sched_wakeup(cos_spd_id(), mgr->thdid);
		}
		mgr = mgr->next_mgr;
	}
done:
	RELEASE();
	return 0;
err:
	printc("Memory Pool: TMEM MGR %d returning non-exist memory %p!\n", d_spdid, mgr_addr);
	goto done;
}

#define HAVE_FREE_PAGE (free_page_list != NULL)

static inline struct page_item *freelist_remove()
{
	struct page_item *page = free_page_list;
	/* get from free list */
	assert(HAVE_FREE_PAGE);
	free_page_list = page->free_next;
	return page;
}

void * mempool_get_mem(spdid_t d_spdid, int pages)
{
	/* TODO: getting multiple pages */
	struct page_item *page;
	struct tmem_mgr *mgr;
	void *mgr_addr = NULL;

	TAKE();

	mgr = all_tmem_mgr[d_spdid];
	if (unlikely(!mgr)) goto err;

	if (HAVE_FREE_PAGE) {
		page = freelist_remove();
	} else {
		mgr->glb_blked++;
		goto done;
	}

	mgr_addr = valloc_alloc(cos_spd_id(), d_spdid, 1);
	if (unlikely(!mgr_addr)) {
		printc("Cannot valloc for comp d_spdid!\n");
		goto err1;
	}

	if (unlikely(!mman_alias_page(cos_spd_id(), (vaddr_t)page->l_addr, d_spdid, (vaddr_t)mgr_addr, MAPPING_RW))) 
		goto err2;

        /* cos_vect_add_id(cos_vect *v, void *val, long id) */
	cos_vect_add_id(mgr->mgr_page_descs, (void *)page, (u32_t)mgr_addr>>PAGE_ORDER);

	mgr->mgr_allocated++;

done:
	RELEASE();
	return mgr_addr;
err2:
	printc("Memory Pool: Cannot alias page to client at %x!\n", (unsigned int)mgr_addr);
	valloc_free(cos_spd_id(), d_spdid, mgr_addr, 1);
	mgr_addr = NULL;
err1:
	freelist_add(page);
err:
	printc("Memory Pool: Cannot allocate page to Component %d!\n", d_spdid);
	goto done;
}

/* To avoid unnecessary wake up of the register thread, mgr should call
 * this clear function when it wakes up all its global blocked
 * threads */
int mempool_clear_glb_blked(spdid_t spdid)
{
	struct tmem_mgr *mgr;
	int ret = 0;

	TAKE();
	mgr = all_tmem_mgr[spdid];
	if (unlikely(!mgr)) goto err;
	mgr->glb_blked = 0;
done:
	RELEASE();
	return ret;
err:
	ret = -1;
	goto done;
}

int mempool_set_mgr_desired(spdid_t spdid, int desired)
{
	struct tmem_mgr *mgr;
	int ret = 0;

	TAKE();
	mgr = all_tmem_mgr[spdid];
	if (unlikely(!mgr)) goto err;
	mgr->mgr_desired = desired;
done:
	RELEASE();
	return ret;
err:
	ret = -1;
	goto done;
}

static inline void mempoolmem_mgr_register(spdid_t spdid)
{
	struct tmem_mgr *mgr;

	TAKE();
	assert(unregistered_mgrs > 0);
	assert(!all_tmem_mgr[spdid]);

	all_tmem_mgr[spdid] = malloc(sizeof(struct tmem_mgr));
	mgr = all_tmem_mgr[spdid];
	assert(mgr);

	mgr->mgr_page_descs = page_descs[--unregistered_mgrs];
	mgr->spdid = spdid;
	mgr->mgr_allocated = 0;
	/* Starts with no desired limit. (u32_t) -1 */
	mgr->mgr_desired = -1;
	mgr->thdid = cos_get_thd_id();
	ADDO_MGR_LIST(mgr);

	RELEASE();

	return;
}
int mempool_tmem_mgr_event_waiting(spdid_t spdid)
{
	struct tmem_mgr *mgr;
	if (unlikely(!all_tmem_mgr[spdid]))
		mempoolmem_mgr_register(spdid);

	TAKE();
	mgr = all_tmem_mgr[spdid];
	assert(cos_get_thd_id() == mgr->thdid);
	RELEASE();

	/* shall we set and clear the waiting flag without holding the
	 * lock? Maybe this can avoid some RACE condition, but not totally. */
	mgr->evt_thd_waiting = 1;
	/* Waiting for wake up event here. */
	sched_block(cos_spd_id(), 0);	
	mgr->evt_thd_waiting = 0;

	TAKE();
	all_tmem_mgr[spdid]->glb_blked = 0;
	RELEASE();
	return 0;
}

/**
 * cos_init
 */
void
cos_init(void *arg)
{
	int i;
	struct page_item *mem_item;

	LOCK_INIT();
	
	memset(all_tmem_mgr, 0, sizeof(struct tmem_mgr *) * MAX_NUM_SPDS);
	/* Initialize our free page list */
	for (i = 0; i < MAX_NUM_MEM; i++) {
        
		mem_item = &(all_page_list[i]);
        
		// allocate a page
		mem_item->l_addr = alloc_page();
		if (mem_item->l_addr == NULL){
			printc("<mem_pool>: ERROR, could not allocate page\n");
		} else {
			freelist_add(mem_item);
		}
	}

	return;
}
