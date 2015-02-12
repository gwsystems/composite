#include <cos_component.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_list.h>
#include <cos_vect.h>
#include <cinfo.h>

#include <mem_mgr_large.h>
#include <stkmgr.h>
#include <valloc.h>

#include <tmem.h>
#include <mem_pool.h>

//#define _DEBUG_TMEMMGR

#define WHERESTR  "[file %s, line %d]: "
#define WHEREARG  __FILE__, __LINE__

#define STK_PER_PAGE (PAGE_SIZE/MAX_STACK_SZ)
#define NUM_PAGES (ALL_STACK_SZ/STK_PER_PAGE)

// The total number of stacks
struct cos_stk_item all_stk_list[MAX_NUM_MEM];

struct cos_stk_item *free_mem_in_local_cache(struct spd_tmem_info *sti)
{
	return (struct cos_stk_item *)((sti)->ci.spd_cinfo_page->cos_stacks.freelists[0].freelist);
}

struct cos_stk_item *alloc_item_data_struct(void *l_addr)
{
	struct cos_stk_item *csi;
	csi = malloc(sizeof(struct cos_stk_item));
	if (!csi) BUG();

	INIT_LIST(csi, next, prev);
        
	csi->hptr = l_addr;
	// figure out or location of the top of the stack
	csi->stk = (struct cos_stk *)D_COS_STK_ADDR((char *)csi->hptr);

	return csi;
}

void free_item_data_struct(struct cos_stk_item *csi)
{
	free(csi);
}

static inline struct cos_stk_item *
stkmgr_get_spds_stk_item(struct spd_tmem_info *sti, vaddr_t a)
{
	struct cos_stk_item *csi;
	vaddr_t ra = round_to_page(a);

	for (csi = FIRST_LIST(&sti->tmem_list, next, prev) ;
	     csi != &sti->tmem_list ;
	     csi = FIRST_LIST(csi, next, prev)) {
		if (csi->d_addr == ra) return csi;
	}

	return NULL;
}

static inline int
spd_freelist_add(struct spd_tmem_info *sti, struct cos_stk_item *csi)
{
	/* Should either belong to this spd, or not to another (we
	 * don't want it mapped into two components) */
	assert(csi->parent_spdid == sti->spdid || EMPTY_LIST(csi, next, prev));
	assert(sti->ci.spd_cinfo_page);

	/* FIXME: race */
	csi->stk->next = (struct cos_stk*)sti->ci.spd_cinfo_page->cos_stacks.freelists[0].freelist;
	(sti->ci.spd_cinfo_page)->cos_stacks.freelists[0].freelist = D_COS_STK_ADDR(csi->d_addr);

	return 0;
}

void
mgr_map_client_mem(struct cos_stk_item *csi, struct spd_tmem_info *info)
{
	vaddr_t d_addr, stk_addr;
	spdid_t d_spdid;
	assert(info && csi);
	assert(EMPTY_LIST(csi, next, prev));

	d_spdid = info->spdid;
	
	d_addr = (vaddr_t)valloc_alloc(cos_spd_id(), d_spdid, 1);
	
//	DOUT("Setting flags and astigning flags\n");
	csi->stk->flags = 0xDEADBEEF;
	csi->stk->next = (void *)0xDEADBEEF;
	stk_addr = (vaddr_t)(csi->hptr);
	if (unlikely(d_addr != mman_alias_page(cos_spd_id(), stk_addr, d_spdid, d_addr, MAPPING_RW))){
		printc("<stkmgr>: Unable to map stack into component");
		BUG();
	}
//	DOUT("Mapped page\n");
	csi->d_addr = d_addr;
	csi->parent_spdid = d_spdid;
    
	spd_freelist_add(info, csi);
	return;
}

static inline struct cos_stk_item *
spd_freelist_remove(struct spd_tmem_info *sti)
{
	struct cos_stk *stk;
	struct cos_stk_item *csi;

	stk = (struct cos_stk *)sti->ci.spd_cinfo_page->cos_stacks.freelists[0].freelist;
	if(stk == NULL) return NULL;

	csi = stkmgr_get_spds_stk_item(sti, (vaddr_t)stk);
	assert(csi);
	stk = csi->stk; 	/* convert to local address */
	/* FIXME: race condition */
	sti->ci.spd_cinfo_page->cos_stacks.freelists[0].freelist = (vaddr_t)stk->next;

	return csi;
}

struct cos_stk_item *
mgr_get_client_mem(struct spd_tmem_info *sti)
{
	spdid_t s_spdid;
	struct cos_stk_item * stk_item;
	
	assert(sti);
	stk_item = spd_freelist_remove(sti);

	if (!stk_item)
		return NULL;
	s_spdid = sti->spdid;
	DOUT("Releasing Stack\n");
	mman_revoke_page(cos_spd_id(), (vaddr_t)(stk_item->hptr), 0); 
	valloc_free(cos_spd_id(), s_spdid, (void *)stk_item->d_addr, 1);
	DOUT("Putting stack back on free list\n");
	
	// cause underflow for MAX Int
	stk_item->parent_spdid = 0;
	
	// Clear our memory to prevent leakage
	memset(stk_item->hptr, 0, PAGE_SIZE);
	
	DOUT("Removing from local list\n");
	// remove from s_spdid's stk_list;
	REM_LIST(stk_item, next, prev);
	sti->num_allocated--;
	if (sti->num_allocated == 0) empty_comps++;
	if (sti->num_allocated >= sti->num_desired) over_quota_total--;
	assert(sti->num_allocated == tmem_num_alloc_tmems(s_spdid));

	return stk_item;
}

/**
 * cos_init
 */
void 
cos_init(void *arg){
	int i;

	DOUT("stk mgr running.....\n");
	DOUT("<stkmgr>: in cos_init, thd %d on core %ld\n", cos_get_thd_id(), cos_cpuid());
	LOCK_INIT();

	memset(spd_tmem_info_list, 0, sizeof(struct spd_tmem_info) * MAX_NUM_SPDS);
    
	for(i = 0; i < MAX_NUM_SPDS; i++){
		spd_tmem_info_list[i].spdid = i;    
		INIT_LIST(&spd_tmem_info_list[i].tmem_list, next, prev);
		INIT_LIST(&spd_tmem_info_list[i].bthd_list, next, prev);
	}

	free_tmem_list = NULL;
	INIT_LIST(&global_blk_list, next, prev);

	tmems_allocated = 0;

	over_quota_total = 0;
	over_quota_limit = MAX_NUM_MEM;
	DOUT("Done mapping components information pages!\n");

	DOUT("<stkmgr>: init finished, call event waiting!\n");
	event_waiting();

	return;
}

void
stkmgr_return_stack(spdid_t s_spdid, vaddr_t addr)
{
	/* addr is the address of the stack. no longer needed but keep
	 * it here. */

	struct spd_tmem_info *sti;

	TAKE();
//	printc("start returning!\n");
	sti = get_spd_info(s_spdid);
	assert(sti);

	DOUT("Return of s_spdid is: %d from thd: %d\n", s_spdid,
	     cos_get_thd_id());

	return_tmem(sti);
//	printc("finished return!");
	RELEASE();
}


int
resolve_dependency(struct spd_tmem_info *sti, int skip_stk)
{
	struct cos_stk_item *stk_item;
	int ret = -1;

	if (EMPTY_LIST(&sti->tmem_list, next, prev)) printc("thd%d  @ spd %d\n", cos_get_thd_id(), sti->spdid);
	assert(!EMPTY_LIST(&sti->tmem_list, next, prev));
	for(stk_item = FIRST_LIST(&sti->tmem_list, next, prev);
	    stk_item != &sti->tmem_list && skip_stk > 0; 
	    stk_item = FIRST_LIST(stk_item, next, prev), skip_stk--) ;

	if (stk_item == &sti->tmem_list) goto done;

	/* Remove the assert? thdid_owner is postibly to be 0, which
	 * means there is available stacks in the local freelist. */

	assert(stk_item->stk->thdid_owner != cos_get_thd_id());
	if (!(stk_item->stk->flags & IN_USE)) goto cache;
	assert(stk_item->stk->thdid_owner != 0);

	ret =  stk_item->stk->thdid_owner;
done:
	return ret;
cache:
	/* printc("local cache found!\n"); */
	ret = -2;
	goto done;
}

void mgr_clear_touched_flag(struct spd_tmem_info *sti)
{
	struct cos_stk_item *csi;

	for (csi = FIRST_LIST(&sti->tmem_list, next, prev) ; 
	     csi != &sti->tmem_list ; 
	     csi = FIRST_LIST(csi, next, prev)) {
		if (!(csi->stk->flags & IN_USE)) {
			csi->stk->flags &= ~TOUCHED;
		} else {
			assert(csi->stk->flags & TOUCHED); 
		}		
	}

	return;
}

/**
 * grant a stack to an address
 */
void *
stkmgr_grant_stack(spdid_t d_spdid)
{
	struct spd_tmem_info *info;

	TAKE();

	info = get_spd_info(d_spdid);

	DOUT("<stkmgr>: stkmgr_grant_stack for, spdid: %d, thdid %d\n",
	       d_spdid, cos_get_thd_id());

	assert(SPD_IS_MANAGED(info));

	/* Apply for transient memory. Might block! */
	tmem_grant(info);
	
	RELEASE();

	//DOUT("Returning Stack address: %X\n",(unsigned int)ret);

	return NULL;
}

