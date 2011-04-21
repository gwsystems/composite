#include <cos_component.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_list.h>
#include <mem_mgr.h>
#include <cos_vect.h>
#include <sched.h>
#include <cinfo.h>

#include <stkmgr.h>

//#define _DEBUG_STKMGR

#define WHERESTR  "[file %s, line %d]: "
#define WHEREARG  __FILE__, __LINE__

#ifdef _DEBUG_STKMGR
#define DOUT(fmt,...) printc(WHERESTR fmt, WHEREARG, ##__VA_ARGS__)
#else
#define DOUT(fmt, ...)
#endif

#define STK_PER_PAGE (PAGE_SIZE/MAX_STACK_SZ)
#define NUM_PAGES (ALL_STACK_SZ/STK_PER_PAGE)

#define MAX_BLKED  10
#define DEFAULT_TARGET_ALLOC 10

#define TAKE() if(sched_component_take(cos_spd_id())) BUG();
#define RELEASE() if(sched_component_release(cos_spd_id())) BUG();

/** 
 * Flags to control stack
 */
enum stk_flags {
	IN_USE      = (0x01 << 0),
	RELINQUISH  = (0x01 << 1),
	PERMANATE   = (0x01 << 2),
	MONITOR     = (0x01 << 3),
};

/**
 * This struct maps directly to how the memory
 * is layed out and used in memory
 */
struct cos_stk {
	struct cos_stk *next;
	unsigned int flags;
};

#define D_COS_STK_ADDR(d_addr) (d_addr + PAGE_SIZE - sizeof(struct cos_stk))

/**
 * Information aobut a stack
 */
struct cos_stk_item {
	struct cos_stk_item *next, *prev; /* per-spd list */
	struct cos_stk_item *free_next;
	spdid_t parent_spdid;       // Not needed but saves on lookup
	vaddr_t d_addr;
	void *hptr;
	struct cos_stk *stk;
};

/**
 * keep track of thread id's
 * Should this be a typedef'd type?
 */
struct blocked_thd {
	unsigned short int thd_id;
	struct blocked_thd *next, *prev;
};

/**
 * This structure is used to keep
 * track of information and stats about each
 * spd
 */
struct spd_stk_info {
	spdid_t spdid;
	/* Shared page between the target component, and us */
	struct cos_component_information *ci;
	/* The number of stacks in use by spd, and the number want it
	 * to use, and at any point in time the number of threads in
	 * the blocked list. */
	unsigned int num_allocated, num_desired;
	unsigned int num_blocked_thds,num_waiting_thds;

	/* Measurements */
	unsigned int nthd_blks[MAX_NUM_THREADS];
	u64_t        thd_blk_start[MAX_NUM_THREADS];
	u64_t        thd_blk_tot[MAX_NUM_THREADS];
	unsigned int stat_thd_blk[MAX_BLKED];

	/* stacks and blocked threads */
	struct cos_stk_item stk_list;
	struct blocked_thd bthd_list;
};

void 
stkmgr_update_stats_block(struct spd_stk_info *ssi, unsigned short int tid)
{
	u64_t start;
	int blked = ssi->num_blocked_thds + 1; /* +1 for us */

	/* printc("************** dude, %d blocked my car in %d (nblocked %d) *****************\n",   */
	/*         tid, ssi->spdid, blked);  */
	ssi->nthd_blks[tid]++;
	rdtscll(start);
	ssi->thd_blk_start[tid] = start;
	if (MAX_BLKED <= blked) blked = MAX_BLKED-1;
	ssi->stat_thd_blk[blked]++;
}

void
stkmgr_update_stats_wakeup(struct spd_stk_info *ssi, unsigned short int tid)
{
	u64_t end, tot;

	/* printc("************** dude, %d found my car in %d *****************\n",   */
	/*         tid, ssi->spdid);  */
	rdtscll(end);
	tot = end - ssi->thd_blk_start[tid];
	ssi->thd_blk_tot[tid] += tot;
	ssi->thd_blk_start[tid] = 0;
}

void stkmgr_reset_stats(struct spd_stk_info *ssi)
{
	int i;
	BUG();
	for (i = 0 ; i < MAX_NUM_THREADS ; i++) {
		ssi->nthd_blks[i] = 0;
		ssi->thd_blk_tot[i] = 0;
		ssi->thd_blk_start[i] = 0;
	}
	for (i = 0 ; i < MAX_BLKED ; i++) {
		ssi->stat_thd_blk[i] = 0;
	}
}

// The total number of stacks
struct cos_stk_item all_stk_list[MAX_NUM_STACKS];
// Holds all currently free stacks
struct cos_stk_item *free_stack_list = NULL;
int stacks_allocated, stacks_target;

static inline int
freelist_add(struct cos_stk_item *csi)
{
	assert(EMPTY_LIST(csi, next, prev));
	assert(csi->parent_spdid == 0);

	stacks_allocated--;
	csi->free_next = free_stack_list;
	free_stack_list = csi;

	return 0;
}

static inline struct cos_stk_item *
freelist_remove(void)
{
	struct cos_stk_item *csi;

	if (stacks_allocated >= stacks_target) return NULL;
	csi = free_stack_list;
	if (!csi) return NULL;
	free_stack_list = csi->free_next;
	stacks_allocated++;

	return csi;
}

// Holds info about stack usage
struct spd_stk_info spd_stk_info_list[MAX_NUM_SPDS];

static void stkmgr_print_ci_freelist(void);

#define SPD_HAS_BLK_THD(spd_stk_info) ((spd_stk_info)->num_blocked_thds != 0)
#define SPD_IS_MANAGED(spd_stk_info) ((spd_stk_info)->ci != NULL)

static inline struct spd_stk_info *
get_spd_stk_info(spdid_t spdid)
{
	struct spd_stk_info *ssi;

	if (spdid >= MAX_NUM_SPDS) BUG();
	ssi = &spd_stk_info_list[spdid];
	
	return ssi;
}

static inline vaddr_t 
spd_freelist_value(spdid_t spdid)
{
	struct spd_stk_info *ssi = get_spd_stk_info(spdid);

	assert(ssi->ci);
	return ssi->ci->cos_stacks.freelists[0].freelist;
}

static inline int
spd_freelist_add(spdid_t spdid, struct cos_stk_item *csi)
{
	struct spd_stk_info *ssi = get_spd_stk_info(spdid);

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

	ssi = get_spd_stk_info(spdid);
	for (csi = FIRST_LIST(&ssi->stk_list, next, prev) ;
	     csi != &ssi->stk_list ;
	     csi = FIRST_LIST(csi, next, prev)) {
		if (csi->d_addr == ra) return csi;
	}

	return NULL;
}

static inline struct cos_stk_item *
spd_freelist_remove(spdid_t spdid)
{
	struct cos_stk *stk;
	struct cos_stk_item *csi;
	struct spd_stk_info *ssi;

	ssi = get_spd_stk_info(spdid);
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

static inline int
spd_freelist_empty(spdid_t spdid)
{
	return (0 == spd_freelist_value(spdid));
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
		INIT_LIST(&spd_stk_info_list[i].stk_list, next, prev);
		INIT_LIST(&spd_stk_info_list[i].bthd_list, next, prev);
	}

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
	void *hp = cos_get_heap_ptr();
	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		spdid_t spdid;

		cos_set_heap_ptr((void*)(((unsigned long)hp)+PAGE_SIZE));
		spdid = cinfo_get_spdid(i);
		if (!spdid) break;

		if(cinfo_map(cos_spd_id(), (vaddr_t)hp, spdid)){
			DOUT("Could not map cinfo page for %d\n", spdid);
			BUG();
		}
		spd_stk_info_list[spdid].ci = hp; 
        
		DOUT("mapped -- id: %ld, hp:%x, sp:%x\n",
		     spd_stk_info_list[spdid].ci->cos_this_spd_id, 
		     (unsigned int)spd_stk_info_list[spdid].ci->cos_heap_ptr,
		     (unsigned int)spd_stk_info_list[spdid].ci->cos_stacks.freelists[0].freelist);
    
		hp = cos_get_heap_ptr();

		stacks_target += DEFAULT_TARGET_ALLOC;
		spd_stk_info_list[spdid].num_allocated = 0;
		spd_stk_info_list[spdid].num_desired = DEFAULT_TARGET_ALLOC;
		spd_stk_info_list[spdid].num_blocked_thds = 0;
		spd_stk_info_list[spdid].num_waiting_thds = 0;
	}
	
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

void blklist_wake_threads(struct blocked_thd *bl)
{
	struct blocked_thd *bthd, *bthd_next;
	spdid_t spdid;

	// Wake up 
	spdid = cos_spd_id();
	DOUT("waking up threads for spd %d\n", spdid);
    
	for(bthd = FIRST_LIST(bl, next, prev) ; bthd != bl ; bthd = bthd_next){
		unsigned short int tid;

		bthd_next = FIRST_LIST(bthd, next, prev);
		DOUT("\tWakeing UP thd: %d", bthd->thd_id);
		REM_LIST(bthd, next, prev);
		tid = bthd->thd_id;
		free(bthd);
		sched_wakeup(cos_spd_id(), tid);        
		DOUT("......UP\n");
	}
    
	DOUT("All thds now awake\n");
}

void spd_wake_threads(spdid_t spdid)
{
	struct spd_stk_info *ssi;

	ssi = get_spd_stk_info(spdid);
	/* printc("************ waking up %d threads for spd %d ************\n", */
	/*         ssi->num_blocked_thds, spdid);  */
	blklist_wake_threads(&ssi->bthd_list);
	assert(EMPTY_LIST(&ssi->bthd_list, next, prev));
	ssi->num_blocked_thds = 0;
}


/**
 * gets the number of stacks associated with a given
 * spdid
 */
static unsigned int
stkmgr_num_alloc_stks(spdid_t s_spdid)
{
	int count = 0;
	struct cos_stk_item *stk_item, *list;

	if(s_spdid > MAX_NUM_SPDS) BUG();
    
	list = &spd_stk_info_list[s_spdid].stk_list;
	for (stk_item = FIRST_LIST(list, next, prev) ; 
	     stk_item != list ; 
	     stk_item = FIRST_LIST(stk_item, next, prev)) {
		count++;
	}
    
	return count;
}

/* the stack should NOT be on the freelist within the spd */
static int
stkmgr_stk_remove_from_spd(struct cos_stk_item *stk_item, struct spd_stk_info *ssi)
{
	spdid_t s_spdid;

	s_spdid = ssi->spdid;
	DOUT("Releasing Stack\n");
	mman_revoke_page(cos_spd_id(), (vaddr_t)(stk_item->hptr), 0); 
	DOUT("Putting stack back on free list\n");
	
	// cause underflow for MAX Int
	stk_item->parent_spdid = 0;
	
	// Clear our memory to prevent leakage
	memset(stk_item->hptr, 0, PAGE_SIZE);
	
	DOUT("Removing from local list\n");
	// remove from s_spdid's stk_list;
	REM_LIST(stk_item, next, prev);
	ssi->num_allocated--;
	assert(ssi->num_allocated == stkmgr_num_alloc_stks(s_spdid));

	return 0;
}

/* Return the top address of the page it is mapped into the
 * component */
static vaddr_t
stkmgr_stk_add_to_spd(struct cos_stk_item *stk_item, struct spd_stk_info *info)
{
	vaddr_t d_addr, stk_addr, ret;
	spdid_t d_spdid;
	assert(info && stk_item);
	assert(EMPTY_LIST(stk_item, next, prev));

	d_spdid = info->spdid;
	// FIXME:  Race condition
	d_addr = info->ci->cos_heap_ptr; 
	info->ci->cos_heap_ptr += PAGE_SIZE;
	ret = info->ci->cos_heap_ptr;

//	DOUT("Setting flags and assigning flags\n");
	stk_item->stk->flags = 0xDEADBEEF;
	stk_item->stk->next = (void *)0xDEADBEEF;
	stk_addr = (vaddr_t)(stk_item->hptr);
	if(d_addr != mman_alias_page(cos_spd_id(), stk_addr, d_spdid, d_addr)){
		printc("<stkmgr>: Unable to map stack into component");
		BUG();
	}
//	DOUT("Mapped page\n");
	stk_item->d_addr = d_addr;
	stk_item->parent_spdid = d_spdid;
    
	// Add stack to allocated stack array
//	DOUT("Adding to local spdid stk list\n");
	ADD_LIST(&info->stk_list, stk_item, next, prev); 
	info->num_allocated++;
	assert(info->num_allocated == stkmgr_num_alloc_stks(info->spdid));

	return ret;
}

/* 
 * Is there a component with blocked threads?  Which is the one with
 * the largest disparity between the number of stacks it has, and the
 * number it is supposed to have?
 */
static struct spd_stk_info *
stkmgr_find_spd_requiring_stk(void)
{
	int i, max_required = 0;
	struct spd_stk_info *best = NULL;

	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		struct spd_stk_info *ssi = &spd_stk_info_list[i];
		if (!SPD_IS_MANAGED(ssi)) continue;

		/* priority goes to spds with blocked threads */
		if (SPD_HAS_BLK_THD(ssi) && ssi->num_desired > ssi->num_allocated) {
			int diff = ssi->num_desired - ssi->num_allocated;

			if (max_required < diff) {
				best = ssi;
				max_required = diff;
			}
		}
	}
	return best;
}


static int
stkmgr_stack_find_home(struct cos_stk_item *csi, struct spd_stk_info *prev)
{
	struct spd_stk_info *dest;

	assert(EMPTY_LIST(csi, next, prev));
	dest = stkmgr_find_spd_requiring_stk();
	if (!dest) {
		freelist_add(csi);
	} else {
		assert(SPD_HAS_BLK_THD(dest));
		stkmgr_stk_add_to_spd(csi, dest);
		spd_freelist_add(dest->spdid, csi);
		spd_wake_threads(dest->spdid);
	}
	return 0;
}

static void
stkmgr_spd_unmark_relinquish(struct spd_stk_info *ssi)
{
	struct cos_stk_item *stk_item;

	DOUT("Unmarking relinquish for %d\n", ssi->spdid);
	
	for(stk_item = FIRST_LIST(&ssi->stk_list, next, prev);
	    stk_item != &ssi->stk_list; 
	    stk_item = FIRST_LIST(stk_item, next, prev)){
		stk_item->stk->flags &= ~RELINQUISH;
	}
}

static void 
stkmgr_stack_remove_and_find_home(struct spd_stk_info *ssi, struct cos_stk_item *csi)
{
	stkmgr_stk_remove_from_spd(csi, ssi);
	stkmgr_stack_find_home(csi, ssi);
}

/**
 * Give a stack back to the stk_mgr.  Assume that the stack is NOT on
 * the component's freelist.
 */
static void
__stkmgr_return_stack(struct spd_stk_info *ssi, struct cos_stk_item *stk_item)
{
	spdid_t s_spdid;

	assert(stk_item && ssi);
	s_spdid = ssi->spdid;
	DOUT("$$$$$: %X\n", (unsigned int)stk_item->d_addr); 
	DOUT("Return of s_spdid is: %d from thd: %d\n", s_spdid,
	     cos_get_thd_id());

	/* Don't move the stack if it should be here! */
	if (ssi->num_desired >= ssi->num_allocated) {
		/* restore in component's freelist */
		spd_freelist_add(s_spdid, stk_item);
		/* wake threads! */
		spd_wake_threads(s_spdid);
		if (!SPD_HAS_BLK_THD(ssi)) {
			/* we're under or at quota, and there are no
			 * blocked threads, no more relinquishing! */
			stkmgr_spd_unmark_relinquish(ssi);
		}
	} else {
		stkmgr_stack_remove_and_find_home(ssi, stk_item);
	}
}

void
stkmgr_return_stack(spdid_t s_spdid, vaddr_t addr)
{
	struct cos_stk_item *stk_item;
	struct spd_stk_info *ssi;

	addr -= sizeof(struct cos_stk_item);
	DOUT("component %d returned stack @ %x\n", s_spdid, (unsigned int)addr);
	TAKE();
	ssi = get_spd_stk_info(s_spdid);
	assert(ssi);
	stk_item = stkmgr_get_spds_stk_item(s_spdid, addr);
	/* FIXME: proper error reporting... */
	if (stk_item == NULL) BUG();

	__stkmgr_return_stack(ssi, stk_item);
	RELEASE();
}

/**
 * returns 0 on success
 */
int
stkmgr_revoke_stk_from(spdid_t spdid)
{
	struct cos_stk_item *stk_item;
	struct spd_stk_info *ssi;

	ssi = get_spd_stk_info(spdid);

	/* Is there a stack on the component's freelist? */
	stk_item = spd_freelist_remove(spdid);
	if(stk_item == NULL) return -1;

	DOUT("revoking stack @ %x, switching freelist to %x.\n",
	       (unsigned int)stk_item->d_addr, (unsigned int)stk_item->stk->next);
	
	__stkmgr_return_stack(ssi, stk_item);
	
	return 0;
}

static inline void
stkmgr_spd_mark_relinquish(spdid_t spdid)
{
	struct cos_stk_item *stk_item;

	DOUT("stkmgr_request_stk_from spdid: %d\n", spdid);
	
	for(stk_item = FIRST_LIST(&spd_stk_info_list[spdid].stk_list, next, prev);
	    stk_item != &spd_stk_info_list[spdid].stk_list; 
	    stk_item = FIRST_LIST(stk_item, next, prev)){
		stk_item->stk->flags |= RELINQUISH;
	}
}

static void 
stkmgr_spd_remove_stacks(spdid_t spdid, unsigned int n_stks)
{
	struct spd_stk_info *ssi;
	
	ssi = get_spd_stk_info(spdid);
	while (n_stks && !stkmgr_revoke_stk_from(spdid)) {
		//printc(">>> found and removed stack from %d (tid %d)\n", spdid, cos_get_thd_id());
		n_stks--;
	}
	/* if we haven't harvested enough stacks, do so lazily */
	if (n_stks) stkmgr_spd_mark_relinquish(spdid);
}

/**
 * Asks for a stack back from all of the components.  Will release and
 * take the lock.
 */
static void
stkmgr_wait_for_stack(struct spd_stk_info *ssi)
{
	struct blocked_thd *bthd;

	DOUT("stkmgr_request_stack\n");
	stkmgr_spd_mark_relinquish(ssi->spdid);

	DOUT("All stacks for %d set to relinquish, %d waiting\n", ssi->spdid, cos_get_thd_id());
        
	bthd = malloc(sizeof(struct blocked_thd));
	if (bthd == NULL) BUG();

	bthd->thd_id = cos_get_thd_id();
	DOUT("Adding thd to the blocked list: %d\n", bthd->thd_id);
	ADD_LIST(&ssi->bthd_list, bthd, next, prev);
	ssi->num_blocked_thds++;

	RELEASE();

	DOUT("Blocking thread: %d\n", bthd->thd_id);
	/* FIXME: dependencies */
	sched_block(cos_spd_id(), 0);
	TAKE(); 
	DOUT("Thd %d wokeup and is obtaining a stack\n", cos_get_thd_id());

	return;
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
    
	void *hp = cos_get_heap_ptr();
	cos_set_heap_ptr((void*)(((unsigned long)hp)+PAGE_SIZE));

	if(cinfo_map(cos_spd_id(), (vaddr_t)hp, s)){
		DOUT("Could not map cinfo page for %d\n", spdid);
		BUG();
	}
	spd_stk_info_list[spdid].ci = hp;
	DOUT("mapped -- id: %ld, hp:%x, sp:%x\n",
	     spd_stk_info_list[spdid].ci->cos_this_spd_id, 
	     (unsigned int)spd_stk_info_list[spdid].ci->cos_heap_ptr,
	     (unsigned int)spd_stk_info_list[spdid].ci->cos_stacks.freelists[0].freelist);
}


/**
 * grant a stack to an address
 *
 * TODO:
 *  - Keep various heap pointers around instead of incrementign it every time.
 */
void *
stkmgr_grant_stack(spdid_t d_spdid)
{
	struct cos_stk_item *stk_item;
	struct spd_stk_info *info;
	vaddr_t ret;
	int meas = 0;

	TAKE();

	info = get_spd_stk_info(d_spdid);

	DOUT("<stkmgr>: stkmgr_grant_stack for, spdid: %d, thdid %d\n",
	       d_spdid, cos_get_thd_id());
        
	// Make sure we have access to the info page
	if (!SPD_IS_MANAGED(info)) get_cos_info_page(d_spdid);
	assert(SPD_IS_MANAGED(info));

	info->num_waiting_thds++;
	/* 
	 * Is there a stack in the local freelist?  If not, is there
	 * one is the global freelist and we are under quota on
	 * stacks?  Otherwise block!
	 */
	while (NULL == (stk_item = spd_freelist_remove(d_spdid))) {
		if (info->num_allocated < info->num_desired &&
		    NULL != (stk_item = freelist_remove())) {
			stkmgr_stk_add_to_spd(stk_item, info);
			break;
		}
		if (!meas) {
			meas = 1;
			stkmgr_update_stats_block(info, cos_get_thd_id());
		}
		DOUT("Stack list is null, we need to revoke a stack: spdid: %d thdid: %d\n",
		     d_spdid,
		     cos_get_thd_id());
		stkmgr_wait_for_stack(info);
	}
	if (meas) stkmgr_update_stats_wakeup(info, cos_get_thd_id());
	
	ret = stk_item->d_addr + PAGE_SIZE;
	RELEASE();
	
	info->num_waiting_thds--;
	//DOUT("Returning Stack address: %X\n",(unsigned int)ret);

	return (void *)ret;
}

void 
stkmgr_stack_report(void)
{
	TAKE();
//	stkmgr_print_ci_freelist();
	printc("available: %d,\n", stacks_allocated);
	RELEASE();
}

static int 
spd_remove_spare_stacks(struct spd_stk_info *ssi)
{
	struct cos_stk_item *csi;

	csi = spd_freelist_remove(ssi->spdid);
	if (!csi) return -1;
	stkmgr_stack_remove_and_find_home(ssi, csi);

	return 0;
}

int 
stkmgr_set_concurrency(spdid_t spdid, int concur_lvl, int remove_spare)
{
	struct spd_stk_info *ssi;
	int diff, old;

	/* if (concur_lvl > 1) printc("Set concur of %d to %d\n", spdid, concur_lvl); */
	TAKE();
	ssi = get_spd_stk_info(spdid);
	if (!ssi || !SPD_IS_MANAGED(ssi)) goto err;
	if (concur_lvl < 0) goto err;

	old = ssi->num_desired;
	ssi->num_desired = concur_lvl;
	stacks_target += concur_lvl - old;

	diff = ssi->num_allocated - ssi->num_desired;
	if (diff > 0) stkmgr_spd_remove_stacks(spdid, diff);
	if (diff < 0 && SPD_HAS_BLK_THD(ssi)) spd_wake_threads(spdid);

	if (remove_spare) while (!spd_remove_spare_stacks(ssi)) ;

	RELEASE();
	return 0;
err:
	RELEASE();
	return -1;
}

int
stkmgr_spd_concurrency_estimate(spdid_t spdid)
{
	struct spd_stk_info *ssi;
	int i, avg;
	unsigned long tot = 0, cnt = 0;

	TAKE();
	ssi = get_spd_stk_info(spdid);
	if (!ssi || !SPD_IS_MANAGED(ssi)) {
		RELEASE();
		return -1;
	}

	if (ssi->num_allocated < ssi->num_desired && !ssi->num_waiting_thds) {
		assert(!SPD_HAS_BLK_THD(ssi));
		RELEASE();
		return ssi->num_allocated;
	}

	for (i = 0 ; i < MAX_BLKED ; i++) {
		int n = ssi->stat_thd_blk[i];

		tot += (n * i);
		cnt += n;
		ssi->stat_thd_blk[i] = 0;
	}
	if (cnt == 0 && ssi->num_waiting_thds == 0) {
		avg = ssi->num_allocated;
	} else {
		unsigned int blk_hist;

		if (cnt) blk_hist = (tot/cnt) + !(tot%cnt == 0); /* adjust for rounding */
		else     blk_hist = 0;
		
		avg = ssi->num_allocated + (blk_hist > ssi->num_waiting_thds ? 
					    blk_hist : ssi->num_waiting_thds); 
	}
	RELEASE();

	return avg;
}

unsigned long
stkmgr_thd_blk_time(unsigned short int tid, spdid_t spdid, int reset)
{
	struct spd_stk_info *ssi;
	long long a = 0;
	u64_t t;

	TAKE();
	ssi = get_spd_stk_info(spdid);
	if (!ssi || !SPD_IS_MANAGED(ssi) || tid >= MAX_NUM_THREADS) {
		RELEASE();
		return -1;
	}
	/* currently blocked? */
	if (ssi->thd_blk_start[tid]) {
		rdtscll(t);
		a += t - ssi->thd_blk_start[tid];
	}
	if (ssi->nthd_blks[tid]) {
		a = (a + ssi->thd_blk_tot[tid])/ssi->nthd_blks[tid];
	} 
	if (reset) {
		ssi->thd_blk_tot[tid] = 0;
		ssi->nthd_blks[tid] = 0;
	}
	RELEASE();
	
	return (a >> 20) + ! ((a & 1048575) == 0);/* right shift 20 bits and round up, 2^20 - 1 = 1048575 */
}

int
stkmgr_thd_blk_cnt(unsigned short int tid, spdid_t spdid, int reset)
{
	struct spd_stk_info *ssi;
	int n;

	TAKE();
	ssi = get_spd_stk_info(spdid);
	if (!ssi || !SPD_IS_MANAGED(ssi) || tid >= MAX_NUM_THREADS) {
		RELEASE();
		return -1;
	}
	n = ssi->nthd_blks[tid];
	/* Thread on the blocked list? */
	if (ssi->thd_blk_start[tid] && n == 0) n = 1;
	if (reset) {
		ssi->thd_blk_tot[tid] = 0;
		ssi->nthd_blks[tid] = 0;
	}
	RELEASE();
	
	return n;
}

void
stkmgr_spd_meas_reset(void)
{
	struct spd_stk_info *ssi;
	int i;

	TAKE();
	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		ssi = get_spd_stk_info(i);
		if (!ssi) BUG();
		if (!SPD_IS_MANAGED(ssi)) continue;
		
		stkmgr_reset_stats(ssi);
	}
	RELEASE();
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

		for (stk_item = FIRST_LIST(&info->stk_list, next, prev) ;
		     stk_item != &info->stk_list ; 
		     stk_item = FIRST_LIST(stk_item, next, prev)) {
			if (stk_item->stk->flags & IN_USE) cnt++;
		}
		printc("stkmgr: spdid %d w/ %d stacks, %d in use, %d blocked\n", 
		       i, info->num_allocated, cnt, info->num_blocked_thds);
		assert(info->num_allocated == stkmgr_num_alloc_stks(info->spdid));
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

/**
 * This is here just to make sure we get scheduled, it can 
 * most likely be removed now
 */
void
bin(void){
	sched_block(cos_spd_id(), 0);
}

