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

//#define _DEBUG_STKMGR

#define WHERESTR  "[file %s, line %d]: "
#define WHEREARG  __FILE__, __LINE__

#define STK_PER_PAGE (PAGE_SIZE/MAX_STACK_SZ)
#define NUM_PAGES (ALL_STACK_SZ/STK_PER_PAGE)

#define DEFAULT_TARGET_ALLOC 10

/** 
 * Flags to control stack
 */
enum stk_flags {
	IN_USE      = (0x01 << 0),
	RELINQUISH  = (0x01 << 1),
	PERMANATE   = (0x01 << 2),
	MONITOR     = (0x01 << 3),
};

// The total number of stacks
struct cos_stk_item all_stk_list[MAX_NUM_STACKS];

static void stkmgr_print_ci_freelist(void);

/* never used ... */
/*
static inline vaddr_t
spd_freelist_value(spdid_t spdid)
{
	struct spd_stk_info *ssi = get_spd_info(spdid);

	assert(ssi->ci);
	return ssi->ci->cos_stacks.freelists[0].freelist;
}

static inline int
spd_freelist_empty(spdid_t spdid)
{
	return (0 == spd_freelist_value(spdid));
}
*/

inline int
spd_freelist_add(spdid_t spdid, struct cos_stk_item *csi)
{
	struct spd_stk_info *ssi = get_spd_info(spdid);

	/* Should either belong to this spd, or not to another (we
	 * don't want it mapped into two components) */
	assert(csi->parent_spdid == spdid || EMPTY_LIST(csi, next, prev));
	assert(ssi->ci);

	/* FIXME: race */
	csi->stk->next = (struct cos_stk*)ssi->ci->cos_stacks.freelists[0].freelist;
	ssi->ci->cos_stacks.freelists[0].freelist = D_COS_STK_ADDR(csi->d_addr);

	return 0;
}

static inline struct cos_stk_item *
stkmgr_get_spds_stk_item(spdid_t spdid, vaddr_t a)
{
	struct spd_stk_info *ssi;
	struct cos_stk_item *csi;
	vaddr_t ra = round_to_page(a);

	ssi = get_spd_info(spdid);
	for (csi = FIRST_LIST(&ssi->tmem_list, next, prev) ;
	     csi != &ssi->tmem_list ;
	     csi = FIRST_LIST(csi, next, prev)) {
		if (csi->d_addr == ra) return csi;
	}

	return NULL;
}

inline struct cos_stk_item *
spd_freelist_remove(spdid_t spdid)
{
	struct cos_stk *stk;
	struct cos_stk_item *csi;
	struct spd_stk_info *ssi;

	ssi = get_spd_info(spdid);
	stk = (struct cos_stk *)ssi->ci->cos_stacks.freelists[0].freelist;
	if(stk == NULL) return NULL;

	csi = stkmgr_get_spds_stk_item(spdid, (vaddr_t)stk);
	/* FIXME: proper error reporting... */
	if(csi == NULL) BUG();
	stk = csi->stk; 	/* convert to local address */
	/* FIXME: race condition */
	ssi->ci->cos_stacks.freelists[0].freelist = (vaddr_t)stk->next;

	return csi;
}

/**
 * cos_init
 */
void 
cos_init(void *arg){
	int i;
	struct cos_stk_item *stk_item;

	DOUT("<stkmgr>: STACK in cos_init\n");

	memset(spd_stk_info_list, 0, sizeof(struct spd_stk_info) * MAX_NUM_SPDS);
    
	for(i = 0; i < MAX_NUM_SPDS; i++){
		spd_stk_info_list[i].spdid = i;    
		INIT_LIST(&spd_stk_info_list[i].tmem_list, next, prev);
		INIT_LIST(&spd_stk_info_list[i].bthd_list, next, prev);
	}

	free_tmem_list = NULL;
	// Initialize our free stack list
	for(i = 0; i < MAX_NUM_STACKS; i++){
        
		// put stk list is some known state
		stk_item = &(all_stk_list[i]);
		stk_item->stk  = NULL;
		INIT_LIST(stk_item, next, prev);
        
		// allocate a page
		stk_item->hptr = alloc_page();
		if (stk_item->hptr == NULL){
			DOUT("<stk_mgr>: ERROR, could not allocate stack\n"); 
		} else {
			// figure out or location of the top of the stack
			stk_item->stk = (struct cos_stk *)D_COS_STK_ADDR((char *)stk_item->hptr);
			freelist_add(stk_item);
		}
	}
	stacks_allocated = 0;

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
		spd_stk_info_list[spdid].ci = hp; 
		spd_stk_info_list[spdid].managed = 1;

		DOUT("mapped -- id: %ld, hp:%x, sp:%x\n",
		     spd_stk_info_list[spdid].ci->cos_this_spd_id, 
		     (unsigned int)spd_stk_info_list[spdid].ci->cos_heap_ptr,
		     (unsigned int)spd_stk_info_list[spdid].ci->cos_stacks.freelists[0].freelist);
    
		stacks_target += DEFAULT_TARGET_ALLOC;
		spd_stk_info_list[spdid].num_allocated = 0;
		spd_stk_info_list[spdid].num_desired = DEFAULT_TARGET_ALLOC;
		spd_stk_info_list[spdid].num_blocked_thds = 0;
		spd_stk_info_list[spdid].num_waiting_thds = 0;
		spd_stk_info_list[spdid].ss_counter = 0;
		spd_stk_info_list[spdid].ss_max = MAX_NUM_STACKS;
		empty_comps++;
	}
	over_quota = 0;
	over_quota_limit = MAX_NUM_STACKS;
	DOUT("Done mapping components information pages!\n");
	DOUT("<stkmgr>: init finished\n");
	return;
}

/**
 * Assuming that the top of the stack is passed
 */
static inline struct cos_stk_item *
stkmgr_get_cos_stk_item(vaddr_t addr){
	int i;
    
	for(i = 0; i < MAX_NUM_STACKS; i++){
		if(addr == (vaddr_t)D_COS_STK_ADDR(all_stk_list[i].d_addr)){
			return &all_stk_list[i];
		}
	}

	return NULL;
}

/* the stack should NOT be on the freelist within the spd */
int
remove_tmem_from_spd(struct cos_stk_item *stk_item, struct spd_stk_info *ssi)
{
	spdid_t s_spdid;

	s_spdid = ssi->spdid;
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
	ssi->num_allocated--;
	if (ssi->num_allocated == 0) empty_comps++;
	if (ssi->num_allocated >= ssi->num_desired) over_quota--;
	assert(ssi->num_allocated == tmem_num_alloc_stks(s_spdid));

	return 0;
}

/* Return the top address of the page it is mapped into the
 * component */
vaddr_t
add_tmem_to_spd(struct cos_stk_item *csi, struct spd_stk_info *info)
{
	vaddr_t d_addr, stk_addr, ret;
	spdid_t d_spdid;
	assert(info && csi);
	assert(EMPTY_LIST(csi, next, prev));

	d_spdid = info->spdid;
	
	d_addr = (vaddr_t)valloc_alloc(cos_spd_id(), d_spdid, 1);
	ret = d_addr + PAGE_SIZE;
	
//	DOUT("Setting flags and assigning flags\n");
	csi->stk->flags = 0xDEADBEEF;
	csi->stk->next = (void *)0xDEADBEEF;
	stk_addr = (vaddr_t)(csi->hptr);
	if(d_addr != mman_alias_page(cos_spd_id(), stk_addr, d_spdid, d_addr)){
		printc("<stkmgr>: Unable to map stack into component");
		BUG();
	}
//	DOUT("Mapped page\n");
	csi->d_addr = d_addr;
	csi->parent_spdid = d_spdid;
    
	// Add stack to allocated stack array
//	DOUT("Adding to local spdid stk list\n");
	ADD_LIST(&info->tmem_list, csi, next, prev);
	info->num_allocated++;
	if (info->num_allocated == 1) empty_comps--;
	if (info->num_allocated > info->num_desired) over_quota++;
	assert(info->num_allocated == tmem_num_alloc_stks(info->spdid));

	return ret;
}

void
spd_unmark_relinquish(struct spd_stk_info *ssi)
{
	struct cos_stk_item *stk_item;

	DOUT("Unmarking relinquish for %d\n", ssi->spdid);
	
	for(stk_item = FIRST_LIST(&ssi->tmem_list, next, prev);
	    stk_item != &ssi->tmem_list; 
	    stk_item = FIRST_LIST(stk_item, next, prev)){
		stk_item->stk->flags &= ~RELINQUISH;
	}
}

void
stkmgr_return_stack(spdid_t s_spdid, vaddr_t addr)
{
	struct cos_stk_item *stk_item;
	struct spd_stk_info *ssi;

	//addr -= sizeof(struct cos_stk_item);
	DOUT("component %d returned stack @ %x\n", s_spdid, (unsigned int)addr);
	TAKE();
	ssi = get_spd_info(s_spdid);
	assert(ssi);
	stk_item = stkmgr_get_spds_stk_item(s_spdid, addr);
	/* FIXME: proper error reporting... */
	if (stk_item == NULL) BUG();

	stk_item->stk->flags = 0;
	stk_item->stk->thdid_owner = 0;

	DOUT("$$$$$: %X\n", (unsigned int)stk_item->d_addr); 
	DOUT("Return of s_spdid is: %d from thd: %d\n", s_spdid,
	     cos_get_thd_id());

	return_tmem(ssi, stk_item);
	RELEASE();
}

inline void
spd_mark_relinquish(spdid_t spdid)
{
	struct cos_stk_item *stk_item;

	DOUT("stkmgr_request_stk_from spdid: %d\n", spdid);
	
	for(stk_item = FIRST_LIST(&spd_stk_info_list[spdid].tmem_list, next, prev);
	    stk_item != &spd_stk_info_list[spdid].tmem_list; 
	    stk_item = FIRST_LIST(stk_item, next, prev)){
		stk_item->stk->flags |= RELINQUISH;
	}
}

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

	if(spdid > MAX_NUM_SPDS){
		BUG(); 
	}
	for (i = 0; i < MAX_NUM_SPDS; i++) {
		s = cinfo_get_spdid(i);
		if(!s) { 
			printc("Unable to map compoents cinfo page!\n");
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
	spd_stk_info_list[spdid].ci = hp;
	spd_stk_info_list[spdid].managed = 1;

	DOUT("mapped -- id: %ld, hp:%x, sp:%x\n",
	     spd_stk_info_list[spdid].ci->cos_this_spd_id, 
	     (unsigned int)spd_stk_info_list[spdid].ci->cos_heap_ptr,
	     (unsigned int)spd_stk_info_list[spdid].ci->cos_stacks.freelists[0].freelist);
}

u32_t
resolve_dependency(struct spd_stk_info *ssi, int skip_stk)
{
	struct cos_stk_item *stk_item;

	for(stk_item = FIRST_LIST(&ssi->tmem_list, next, prev);
	    stk_item != &ssi->tmem_list && skip_stk > 0; 
	    stk_item = FIRST_LIST(stk_item, next, prev), skip_stk--) {
		assert(stk_item->stk->flags & RELINQUISH);
	}

	if (stk_item == &ssi->tmem_list) return 0;

	assert(stk_item->stk->thdid_owner != 0);
	return stk_item->stk->thdid_owner;
}

/**
 * grant a stack to an address
 */
void *
stkmgr_grant_stack(spdid_t d_spdid)
{
	struct spd_stk_info *info;

	TAKE();

	info = get_spd_info(d_spdid);

	DOUT("<stkmgr>: stkmgr_grant_stack for, spdid: %d, thdid %d\n",
	       d_spdid, cos_get_thd_id());
        
	// Make sure we have access to the info page
	if (!SPD_IS_MANAGED(info)) get_cos_info_page(d_spdid);
	assert(SPD_IS_MANAGED(info));
	
	/* Apply for transient memory. Might block! */
	tmem_contend_mem(info);
	
	/* /\* Here we got the stk already! *\/ */
	/* ret = stk_item->d_addr + PAGE_SIZE - sizeof(struct cos_stk); */

	/* stk_item->stk->flags = IN_USE; */
	/* stk_item->stk->thdid_owner = cos_get_thd_id(); */

	RELEASE();

	//DOUT("Returning Stack address: %X\n",(unsigned int)ret);

	return NULL;
}

void 
stkmgr_stack_report(void)
{
	tmem_report();
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
	if(stk->flags & RELINQUISH){
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
	struct spd_stk_info *info;
	struct cos_stk_item *stk_item;
	void *curr;

	info = &spd_stk_info_list[spdid];
	if (info->ci == NULL) return -1;

	curr = (void *)info->ci->cos_stacks.freelists[0].freelist;
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
	si = stkmgr_get_spds_stk_item(s_spdid, s_addr);
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

int 
stkmgr_stack_close(spdid_t d_spdid, vaddr_t d_addr)
{
	/* the memory manager will ensure that both we and the
	 * destination own this page */
	mman_release_page(d_spdid, d_addr, 0);
	return 0;
}

//#define PRINT_FREELIST_ELEMENTS

static void
stkmgr_print_ci_freelist(void)
{
	int i;
	struct spd_stk_info *info;
	//void *curr;
	struct cos_stk_item *stk_item;//, *p;

	for(i = 0; i < MAX_NUM_SPDS; i++){
		unsigned int cnt = 0;

		info = &spd_stk_info_list[i];
		if(info->ci == NULL) continue;

		if (info->num_allocated == 0 && info->num_blocked_thds == 0) continue;

		for (stk_item = FIRST_LIST(&info->tmem_list, next, prev) ;
		     stk_item != &info->tmem_list ; 
		     stk_item = FIRST_LIST(stk_item, next, prev)) {
			if (stk_item->stk->flags & IN_USE) cnt++;
		}
		printc("stkmgr: spdid %d w/ %d stacks, %d in use, %d blocked\n", 
		       i, info->num_allocated, cnt, info->num_blocked_thds);
		assert(info->num_allocated == tmem_num_alloc_stks(info->spdid));
#ifdef PRINT_FREELIST_ELEMENTS
		curr = (void *)info->ci->cos_stacks.freelists[0].freelist;
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
