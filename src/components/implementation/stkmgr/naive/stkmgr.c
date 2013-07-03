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

#define DEFAULT_TARGET_ALLOC 10

// The total number of stacks
struct cos_stk_item all_stk_list[MAX_NUM_MEM];

static void stkmgr_print_ci_freelist(void);

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
	if(unlikely(d_addr != mman_alias_page(cos_spd_id(), stk_addr, d_spdid, d_addr))){
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

	// Map all of the spds we can into this component
	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		spdid_t spdid;
		void *hp;
		/* hp = cos_get_vas_page(); */
		hp = valloc_alloc(cos_spd_id(), cos_spd_id(), 1);
		spdid = cinfo_get_spdid(i);
		if (!spdid) break;

		if(cinfo_map(cos_spd_id(), (vaddr_t)hp, spdid)){
			DOUT("Could not map cinfo page for %d\n", spdid);
			BUG();
		}
		spd_tmem_info_list[spdid].ci.spd_cinfo_page = hp; 
		/* spd_tmem_info_list[spdid].spd_cinfo_page = hp; */

		spd_tmem_info_list[spdid].managed = 1;

		/* DOUT("mapped -- id: %ld, hp:%x, sp:%x\n", */
		/*      spd_tmem_info_list[spdid].ci->cos_this_spd_id,  */
		/*      (unsigned int)spd_tmem_info_list[spdid].ci->cos_heap_ptr, */
		/*      (unsigned int)spd_tmem_info_list[spdid].ci->cos_stacks.freelists[0].freelist); */
    
		tmems_target += DEFAULT_TARGET_ALLOC;
		spd_tmem_info_list[spdid].num_allocated = 0;
		spd_tmem_info_list[spdid].num_desired = DEFAULT_TARGET_ALLOC;
		spd_tmem_info_list[spdid].num_blocked_thds = 0;
		spd_tmem_info_list[spdid].num_glb_blocked = 0;
		spd_tmem_info_list[spdid].num_waiting_thds = 0;
		spd_tmem_info_list[spdid].ss_counter = 0;
		spd_tmem_info_list[spdid].ss_max = MAX_NUM_MEM;
		empty_comps++;
	}
	over_quota_total = 0;
	over_quota_limit = MAX_NUM_MEM;
	DOUT("Done mapping components information pages!\n");

	DOUT("<stkmgr>: init finished, call event waiting!\n");
	event_waiting();

	return;
}

/**
 * Assuming that the top of the stack is passed
 * Only used for debug and print info.
 */
static inline struct cos_stk_item *
stkmgr_get_cos_stk_item(vaddr_t addr){
	int i;
        /* don't use this as we get memory from mem_pool */
	return NULL;    
	for(i = 0; i < MAX_NUM_MEM; i++){
		if(addr == (vaddr_t)D_COS_STK_ADDR(all_stk_list[i].d_addr)){
			return &all_stk_list[i];
		}
	}
	return NULL;
}

/* void */
/* spd_unmark_relinquish(struct spd_tmem_info *sti) */
/* { */
/* 	struct cos_stk_item *stk_item; */

/* 	DOUT("Unmarking relinquish for %d\n", sti->spdid); */
	
/* 	for(stk_item = FIRST_LIST(&sti->tmem_list, next, prev); */
/* 	    stk_item != &sti->tmem_list;  */
/* 	    stk_item = FIRST_LIST(stk_item, next, prev)){ */
/* 		stk_item->stk->flags &= ~RELINQUISH; */
/* 	} */
/* } */

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

/* inline void */
/* spd_mark_relinquish(struct spd_tmem_info *sti) */
/* { */
/* 	struct cos_stk_item *stk_item; */
/* 	DOUT("stkmgr_request_stk_from spdid: %d\n", sti->spdid); */
	
/* 	for(stk_item = FIRST_LIST(&sti->tmem_list, next, prev); */
/* 	    stk_item != &sti->tmem_list; */
/* 	    stk_item = FIRST_LIST(stk_item, next, prev)){ */
/* 		stk_item->stk->flags |= RELINQUISH; */
/* 	} */
/* } */

/**
 * maps the compoenents spdid info page on startup
 * I do it this way since not every component may require stacks or
 * what spdid's I even have access too.
 * I am not sure if this is the best way to handle this, but it 
 * should work for now.
 */
static inline void
get_cos_info_page(spdid_t spdid)
{
	spdid_t s;
	int i;
	int found = 0;
	void *hp;

	assert(spdid < MAX_NUM_SPDS);

	for (i = 0; i < MAX_NUM_SPDS; i++) {
		s = cinfo_get_spdid(i);
		if(!s) { 
			printc("Unable to map components cinfo page!\n");
			BUG();
		}
            
		if (s == spdid) {
			found = 1;
			break;
		}
	} 
    
	if(!found){
		DOUT("Could not find cinfo for spdid: %d\n", spdid);
		BUG();
	}
    
	/* hp = cos_get_vas_page(); */
	hp = valloc_alloc(cos_spd_id(), cos_spd_id(), 1);
	if(cinfo_map(cos_spd_id(), (vaddr_t)hp, s)){
		DOUT("Could not map cinfo page for %d\n", spdid);
		BUG();
	}
	spd_tmem_info_list[spdid].ci.spd_cinfo_page = hp;

	spd_tmem_info_list[spdid].managed = 1;

	DOUT("mapped -- id: %ld, hp:%x, sp:%x\n",
	     spd_tmem_info_list[spdid].ci.spd_cinfo_page->cos_this_spd_id, 
	     (unsigned int)spd_tmem_info_list[spdid].ci.spd_cinfo_page->cos_heap_ptr,
	     (unsigned int)spd_tmem_info_list[spdid].ci.spd_cinfo_page->cos_stacks.freelists[0].freelist);
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
        
	// Make sure we have access to the info page
	if (!SPD_IS_MANAGED(info)) get_cos_info_page(d_spdid);
	assert(SPD_IS_MANAGED(info));
	
	/* Apply for transient memory. Might block! */
	tmem_grant(info);
	
	RELEASE();

	//DOUT("Returning Stack address: %X\n",(unsigned int)ret);

	return NULL;
}

void 
stkmgr_stack_report(void)
{
	tmem_report();
	/* int i; */
	/* for (i = 0 ; i < MAX_NUM_SPDS ; i++) { */
	/* 	spdid_t spdid; */
	/* 	void *hp; */

	/* 	/\* hp = cos_get_vas_page(); *\/ */
	/* 	spdid = cinfo_get_spdid(i); */
	/* 	if (!spdid) break; */

	/* 	printc("spd %d, allocated %d, desired %d, blk thds %d, glb %d, ss_counter %d\n", */
	/* 	       spdid, */
	/* 	       spd_tmem_info_list[spdid].num_allocated, */
	/* 	       spd_tmem_info_list[spdid].num_desired, */
	/* 	       spd_tmem_info_list[spdid].num_blocked_thds, */
	/* 	       spd_tmem_info_list[spdid].num_glb_blocked, */
	/* 	       spd_tmem_info_list[spdid].ss_counter); */
	/* } */
}

int
stkmgr_set_suspension_limit(spdid_t cid, int max)
{
	return tmem_set_suspension_limit(cid, max);
}

int
stkmgr_detect_suspension(spdid_t cid, int reset)
{
	return tmem_detect_suspension(cid, reset);
}

int
stkmgr_set_over_quota_limit(int limit)
{
	return tmem_set_over_quota_limit(limit);
}

int
stkmgr_get_allocated(spdid_t cid)
{
	return tmem_get_allocated(cid);
}

int
stkmgr_spd_concurrency_estimate(spdid_t spdid)
{
	return tmem_spd_concurrency_estimate(spdid);
}

unsigned long
stkmgr_thd_blk_time(unsigned short int tid, spdid_t spdid, int reset)
{
	return tmem_get_thd_blk_time(tid, spdid, reset);
}

int
stkmgr_thd_blk_cnt(unsigned short int tid, spdid_t spdid, int reset)
{
	return tmem_get_thd_blk_cnt(tid, spdid, reset);
}

void
stkmgr_spd_meas_reset(void)
{
	tmem_spd_meas_reset();
}

int 
stkmgr_set_concurrency(spdid_t spdid, int concur_lvl, int remove_spare)
{
	return tmem_set_concurrency(spdid, concur_lvl, remove_spare);
}

void
print_flags(struct cos_stk *stk)
{
	int p = 0;

	printc("\t\tflags:");
	if(stk->flags & IN_USE){
		printc(" In Use"); p = 1;
	}
	if(stk->flags & TOUCHED){
		printc(" Relinquish"); p = 1;
	}
	if(stk->flags & PERMANATE){
		printc(" Permanate"); p = 1;
	}
	if(stk->flags & MONITOR){
		printc(" Monitor"); p = 1;
	}
	if (!p) printc(" Nil");
	printc("\n");
}

static int
stkmgr_in_freelist(spdid_t spdid, struct cos_stk_item *csi)
{
	struct spd_tmem_info *info;
	struct cos_stk_item *stk_item;
	void *curr;

	info = &spd_tmem_info_list[spdid];
	if (info->ci.spd_cinfo_page == NULL) return -1;

	curr = (void *)info->ci.spd_cinfo_page->cos_stacks.freelists[0].freelist;
	if (curr == NULL) return 0;
	
	stk_item = stkmgr_get_cos_stk_item((vaddr_t)curr);
	while(stk_item) {
		if (stk_item == csi) return 1;
		curr = stk_item->stk->next;
		stk_item = stkmgr_get_cos_stk_item((vaddr_t)curr);    
	}
	return 0;
}

int
stkmgr_stack_introspect(spdid_t d_spdid, vaddr_t d_addr, 
			spdid_t s_spdid, vaddr_t s_addr)
{
	struct cos_stk_item *si;
	int ret = -1;

	TAKE();
	si = stkmgr_get_spds_stk_item(get_spd_info(s_spdid), s_addr);
	if (!si) goto err;
	
	if(d_addr != mman_alias_page(cos_spd_id(), (vaddr_t)si->hptr, d_spdid, d_addr)){
		printc("<stkmgr>: Unable to map stack into component during introspection\n");
		BUG();
	}
	ret = 0;
err:
	RELEASE();
	return ret;
}

//#define PRINT_FREELIST_ELEMENTS

static void
stkmgr_print_ci_freelist(void)
{
	int i;
	struct spd_tmem_info *info;
	//void *curr;
	struct cos_stk_item *stk_item;//, *p;

	for(i = 0; i < MAX_NUM_SPDS; i++){
		unsigned int cnt = 0;

		info = &spd_tmem_info_list[i];
		if(info->ci.spd_cinfo_page == NULL) continue;

		if (info->num_allocated == 0 && info->num_blocked_thds == 0) continue;

		for (stk_item = FIRST_LIST(&info->tmem_list, next, prev) ;
		     stk_item != &info->tmem_list ; 
		     stk_item = FIRST_LIST(stk_item, next, prev)) {
			if (stk_item->stk->flags & IN_USE) cnt++;
		}
		printc("stkmgr: spdid %d w/ %d stacks, %d in use, %d blocked\n", 
		       i, info->num_allocated, cnt, info->num_blocked_thds);
		assert(info->num_allocated == tmem_num_alloc_tmems(info->spdid));
#ifdef PRINT_FREELIST_ELEMENTS
		curr = (void *)info->ci.spd_cinfo_page->cos_stacks.freelists[0].freelist;
		if(curr) {
			DOUT("\tcomponent freelist: %p\n", curr);
			p = stk_item = stkmgr_get_cos_stk_item((vaddr_t)curr);
			while (stk_item) {
				DOUT("\tStack:\n"	\
				       "\t\tcurr: %X\n"	\
				       "\t\taddr: %X\n"	\
				       "\t\tnext: %X\n",
				       (unsigned int)stk_item->stk,
				       (unsigned int)D_COS_STK_ADDR(stk_item->d_addr),
				       (unsigned int)stk_item->stk->next);
				print_flags(stk_item->stk);
				curr = stk_item->stk->next;
				stk_item = stkmgr_get_cos_stk_item((vaddr_t)curr);
				if (p == stk_item) {
					printc("<<WTF: freelist recursion...>>\n");
					break;
				}
				p = stk_item;
			}
		}
		for (stk_item = FIRST_LIST(&info->stk_list, next, prev) ;
		     stk_item != &info->stk_list ;
		     stk_item = FIRST_LIST(stk_item, next, prev)) {
			if (!stkmgr_in_freelist(i, stk_item)) {
				DOUT("\tStack off of freelist:\n"	\
				       "\t\tcurr: %X\n"			\
				       "\t\taddr: %X\n"			\
				       "\t\tnext: %X\n",
				       (unsigned int)stk_item->stk,
				       (unsigned int)D_COS_STK_ADDR(stk_item->d_addr),
				       (unsigned int)stk_item->stk->next);
				print_flags(stk_item->stk);
			}
		}
#endif
	}

}
