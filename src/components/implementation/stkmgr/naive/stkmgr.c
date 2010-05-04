#include <cos_component.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_list.h>
#include <mem_mgr.h>
#include <cos_vect.h>
#include <sched.h>
#include <cinfo.h>

#include <stkmgr.h>

#define _DEBUG_STKMGR

#define WHERESTR  "[file %s, line %d]: "
#define WHEREARG  __FILE__, __LINE__

#ifdef _DEBUG_STKMGR
    #define DOUT(fmt,...) printc(WHERESTR fmt, WHEREARG, ##__VA_ARGS__)
#else
    #define DOUT(fmt, ...)
#endif

#define STK_PER_PAGE (PAGE_SIZE/MAX_STACK_SZ)
#define NUM_PAGES (ALL_STACK_SZ/STK_PER_PAGE)
#define MAX_NUM_STACKS 100 //6    // MAX_NUM_THREADS

#define POW_2_CNT  5        // Should be log_2(MAX_NUM_STACKS)

#define TAKE(spdid) if(sched_component_take(spdid)) BUG();
#define RELEASE(spdid) if(sched_component_release(spdid)) BUG();

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


/**
 * Information aobut a stack
 */
struct cos_stk_item {
    struct cos_stk_item *next;
    struct cos_stk_item *prev;
    spdid_t parent_spdid;       // Not needed but saves on lookup
    vaddr_t d_addr;
    void *hptr;
    struct cos_stk *stk;
};

/**
 * This structure is used to keep
 * track of information and stats about each
 * spd
 */
struct spd_stk_info {
    spdid_t spdid; /* Dont really need */
    struct cos_component_information *ci;
    unsigned int num_grants;
    unsigned int num_returns;
    unsigned int thd_count[MAX_NUM_THREADS];
    unsigned int num_blocked_thds;
    unsigned int stat_thd_blk[POW_2_CNT];
    struct cos_stk_item stk_list;      
};

/**
 * keep track of thread id's
 * Should this be a typedef'd type?
 */
struct blocked_thd {
    unsigned short int thd_id;
    struct blocked_thd *next, *prev;
};


// The total number of stacks
struct cos_stk_item all_stk_list[MAX_NUM_STACKS];

// Holds all currently free stacks
struct cos_stk_item *free_stack_list = NULL;

// Holds info about stack usage
struct spd_stk_info spd_stk_info_list[MAX_NUM_SPDS];

// Global number of blocked thds
static int num_blocked_thds = 0;

// List of blocked threads
struct blocked_thd blocked_thd_list;


void stkmgr_print_ci_freelist(void);
 

/**
 * cos_init
 */
void 
cos_init(void *arg){
    int i;
    struct cos_stk_item *stk_item;

    DOUT("<stkmgr>: STACK in cos_init\n");
   
    INIT_LIST(&blocked_thd_list, next, prev);
    
    memset(spd_stk_info_list, 0, sizeof(struct spd_stk_info) * MAX_NUM_SPDS);
    
    for(i = 0; i < MAX_NUM_SPDS; i++){
        spd_stk_info_list[i].spdid = i;    
        INIT_LIST(&spd_stk_info_list[i].stk_list, next, prev);
    }

    // Initalize our free stack list
    for(i = 0; i < MAX_NUM_STACKS; i++){
        
        // put stk list is some known state
        stk_item = &(all_stk_list[i]);
        stk_item->stk  = NULL;
        
        // allocate a page
        stk_item->hptr = alloc_page();
        if(stk_item->hptr == NULL){
            DOUT("<stk_mgr>: ERROR, could not allocate stack\n"); 
        }else{
         
            // figure out or location of the top of the stack
            stk_item->stk = (struct cos_stk *)(((char *)stk_item->hptr) + PAGE_SIZE - sizeof(struct cos_stk)); 
            // add it to our stack_item and add that to the fre list
            stk_item->next = free_stack_list;
            free_stack_list = stk_item;
        }
    }

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
    }
	
	DOUT("Done mapping components information pages!\n");
    DOUT("<stkmgr>: init finished\n");
    return;
}

static inline struct spd_stk_info *
stkmgr_get_spd_stk_info(struct cos_stk_item *stk_item){

    if(stk_item == NULL){
        BUG();
    }

    if(stk_item->parent_spdid > MAX_NUM_SPDS){
        BUG();
    }
    return &spd_stk_info_list[stk_item->parent_spdid];

}

/**
 * Assuming that the top of the stack is passed
 */
static inline struct cos_stk_item *
stkmgr_get_cos_stk_item(vaddr_t addr){
    int i;
    
    DOUT(" stkmgr_get_cos_stk_item\n");

    for(i = 0; i < MAX_NUM_STACKS; i++){
        /* 
        DOUT("Comparing passed addr: %X, d_addr: %X, hptr: %X, stk: %X\n", 
             (unsigned int)addr,
             (unsigned int)all_stk_list[i].d_addr+PAGE_SIZE,
             (unsigned int)all_stk_list[i].hptr,
             (unsigned int)all_stk_list[i].stk);
        */
        if(addr == (vaddr_t)(all_stk_list[i].d_addr + PAGE_SIZE - sizeof(struct cos_stk))){
            return &all_stk_list[i];
        }
    }

    return NULL;
}


/**
 * Give a stack back to the stk_mgr
 */
void
stkmgr_return_stack(spdid_t s_spdid, vaddr_t addr){
    spdid_t spdid;
    struct cos_stk_item *stk_item;
    struct blocked_thd *bthd, *bthd_next;
    short int found = 0;
    DOUT("$$$$$: %X\n", (unsigned int)addr); 
    DOUT("Return of s_spdid is: %d from thd: %d\n", s_spdid,
    cos_get_thd_id());
    int i; 
    
    // Find which component has this stack so we can unmap it
    stk_item = FIRST_LIST(&spd_stk_info_list[s_spdid].stk_list, next, prev);
    for(; stk_item != &spd_stk_info_list[s_spdid].stk_list; stk_item = stk_item->next){
        DOUT("Comparing spdid: %d,  passed addr: %X, d_addr: %X, hptr: %X\n", 
             (int)spdid,
             (unsigned int)addr,
             (unsigned int)stk_item->d_addr+PAGE_SIZE,
             (unsigned int)stk_item->hptr);
                    
        if(stk_item->d_addr+PAGE_SIZE == addr){
            printc("Found stack item in spdid %d\n", i);
            found = 1;
            break;
        }
    }
    if(found != 1){
        DOUT("Unable to locate stack at address: %X\n", (unsigned int)addr);
        BUG();
    }
    
    DOUT("Releasing Stack\n");
    mman_release_page(s_spdid, (vaddr_t)(stk_item->d_addr), 0); 
    DOUT("Putting stack back on free list\n");
    
    // cause underflow for MAX Int
    stk_item->parent_spdid = -1;

    // Free our memory to prevent leakage
    memset(stk_item->hptr, 0, PAGE_SIZE);
   
    DOUT("Removing from local list\n");
    // remove from s_spdid's stk_list;
    REM_LIST(stk_item, next, prev);

    // add item back onto our free list 
    stk_item->next = free_stack_list;   
    free_stack_list = stk_item; 

    // Wake up 
    DOUT("waking up threads\n");
    spdid = cos_spd_id();
    
    TAKE(spdid);
    
    bthd = FIRST_LIST(&blocked_thd_list, next, prev);
    for(; bthd != &blocked_thd_list; bthd = bthd_next){
        bthd_next = FIRST_LIST(bthd, next, prev);
        DOUT("\tWakeing UP thd: %d", bthd->thd_id);
        REM_LIST(bthd, next, prev);
        free(bthd);
        sched_wakeup(cos_spd_id(), bthd->thd_id);        
        printc(" ......UP\n");
    }
    
    DOUT("All thds now awake\n");
    
    RELEASE(spdid);
}

/** 
 * Not this may crash the running spd, this is not
 * a nice function and should be used wisely
 */
int
stkmgr_force_revoke(spdid_t spdid){
    struct cos_stk_item *stk_item;

    if(spdid > MAX_NUM_SPDS){
        BUG();
    }

    stk_item = FIRST_LIST(&spd_stk_info_list[spdid].stk_list, next, prev);
    if(stk_item == &spd_stk_info_list[spdid].stk_list){
        return -1;
    }

    stkmgr_return_stack(spdid, stk_item->d_addr);

    return 0;
}

/**
 * returns 0 on success
 */
int
stkmgr_revoke_stk_from(spdid_t spdid){
    struct cos_stk_item *stk_item;
    struct cos_stk *stk;
    if(spdid > MAX_NUM_SPDS){
        BUG();
    }

    stk = (struct cos_stk *)spd_stk_info_list[spdid].ci->cos_stacks.freelists[0].freelist;
    if(stk == NULL){
        // No Stacks available to revoke
        return -1;
    }
   
    stk_item = stkmgr_get_cos_stk_item(spd_stk_info_list[spdid].ci->cos_stacks.freelists[0].freelist);
    if(stk_item == NULL){
        DOUT("Could not find stk_item\n");
        BUG();
        return -1;
    }

    stkmgr_return_stack(spdid, stk_item->d_addr);
   
    return 0;
}


/**
 * Moves a stack from 1 spdid to another
 */
int
stkmgr_move_stack(spdid_t s_spdid, vaddr_t s_addr, spdid_t d_spdid, vaddr_t d_addr){
    return 0;
}


/**
 * gets the number of stacks associated with a given
 * spdid
 */
static int
stkmgr_num_alloc_stks(spdid_t s_spdid){
    int count;
    struct cos_stk_item *stk_item;
    
    if(s_spdid > MAX_NUM_SPDS){
        BUG();
    }
    
    count = 0;
    stk_item = FIRST_LIST(&spd_stk_info_list[s_spdid].stk_list, next, prev);
    while(stk_item != &spd_stk_info_list[s_spdid].stk_list){
        count++;
        stk_item = stk_item->next;
    }
    
    return count;
}

static inline void
stkmgr_request_stk_from_spdid(spdid_t spdid){
    struct cos_stk_item *stk_item;

    DOUT("stkmgr_request_stk_from spdid: %d\n", spdid);
    stk_item = FIRST_LIST(&spd_stk_info_list[spdid].stk_list, next, prev);
    for(; stk_item != &spd_stk_info_list[spdid].stk_list; stk_item = stk_item->next){
        stk_item->stk->flags |= RELINQUISH;
    }
}


/**
 * Asks for a stack back
 * from all of the components
 */
struct cos_stk_item *
stkmgr_request_stack(void){
    struct cos_stk_item *stk_item; 
    struct blocked_thd *bthd;
    int i;
    
    DOUT("stkmgr_request_stack\n");
    for(i = 0; i < MAX_NUM_STACKS; i++){
        all_stk_list[i].stk->flags |= RELINQUISH;
    }
    DOUT("All stacks set to relinquish\n");
    
    num_blocked_thds++;
    DOUT("Thd %d is waiting for stack\n", cos_get_thd_id());
    
    bthd = malloc(sizeof(struct blocked_thd));
    if(bthd == NULL){
        printc("Malloc failed\n");
        assert(0);
    }

    spdid_t spdid = cos_spd_id();
    TAKE(spdid); 

    bthd->thd_id = cos_get_thd_id();
    DOUT("Adding thd to the blocked list: %d\n", bthd->thd_id);
    ADD_LIST(&blocked_thd_list, bthd, next, prev);
   
    RELEASE(spdid);

    DOUT("Blocking thread: %d\n", bthd->thd_id);
    sched_block(cos_spd_id(), 0);
    DOUT("Thd %d wokeup and is obtaining a stack\n", cos_get_thd_id());
    

    num_blocked_thds--;
    
    if(num_blocked_thds < 0){
        assert(0);
    }

    if(num_blocked_thds == 0){
        for(i = 0; i < MAX_NUM_STACKS; i++){
            all_stk_list[i].stk->flags &= ~RELINQUISH;
        }
    }

    DOUT("All stacks set back to default\n");
    
    return stk_item;
}


/**
 * maps the compoenents spdid info page on startup
 * I do it this way since not every component may require stacks or
 * what spdid's I even have access too.
 * I am not sure if this is the best way to handle this, but it 
 * should work for now.
 */
static inline void
get_cos_info_page(spdid_t spdid){
    spdid_t s;
    int i;
    int found = 0;

    if(spdid > MAX_NUM_SPDS){
       BUG(); 
    }
    for(i = 0; i < MAX_NUM_SPDS; i++){
        s = cinfo_get_spdid(i);
        if(!s){
            printc("Unable to map compoents cinfo page!\n");
            BUG();
        }
            
        if(s == spdid){
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
stkmgr_grant_stack(spdid_t d_spdid){
    struct cos_stk_item *stk_item;
    struct spd_stk_info *info;
    vaddr_t stk_addr,d_addr;
    vaddr_t ret;
    if(d_spdid > MAX_NUM_SPDS){
        assert(0);
    }

    printc("<stkmgr>: stkmgr_grant_stack for, spdid: %d\n",
           d_spdid);
        
    // Make sure we have access to the info page
    info = &spd_stk_info_list[d_spdid];
    if(info->ci == NULL){
        get_cos_info_page(d_spdid);
    }

    // Get a Stack.
    while(free_stack_list == NULL){
        DOUT("Stack list is null, we need to revoke a stack: spdid: %d thdid: %d\n",
             d_spdid,
             cos_get_thd_id());
        stkmgr_request_stack();
    }

    stk_item = free_stack_list;
    free_stack_list = free_stack_list->next;
        
    DOUT("Spdid: %d, Thd: %d Obtained a stack\n",
         d_spdid,
         cos_get_thd_id());
   
    // FIXME:  Race condition
    d_addr = info->ci->cos_heap_ptr; 
    info->ci->cos_heap_ptr += PAGE_SIZE;
    ret = info->ci->cos_heap_ptr;

    DOUT("Setting flags and assigning flags\n");
    stk_item->stk->flags = 0xDEADBEEF;
    stk_item->stk->next = (void *)0xDEADBEEF;
    stk_addr = (vaddr_t)(stk_item->hptr);
    if(d_addr != mman_alias_page(cos_spd_id(), stk_addr, d_spdid, d_addr)){
        printc("<stkmgr>: Unable to map stack into component");
        BUG();
    }
    DOUT("Mapped page\n");
    stk_item->d_addr = d_addr;
    stk_item->parent_spdid = d_spdid;
    
    // Add stack to allocated stack array
    DOUT("Adding to local spdid stk list\n");
    ADD_LIST(&spd_stk_info_list[d_spdid].stk_list, stk_item, next, prev); 
         
    info->thd_count[cos_get_thd_id()]++;
        
    DOUT("Returning Stack address: %X\n",(unsigned int)d_addr);

    stkmgr_print_ci_freelist();
 
    return (void *)ret;
}

void
print_flags(struct cos_stk *stk){
   
    printc("flags:");
    if(stk->flags & IN_USE){
        printc(" In Use");
    }
    if(stk->flags & RELINQUISH){
        printc(" Relinquish");
    }
    if(stk->flags & PERMANATE){
        printc(" Permanate");
    }
    if(stk->flags & MONITOR){
        printc(" Monitor");
    }
    printc("\n");
}

void
stkmgr_print_ci_freelist(void){
    int i;
    struct spd_stk_info *info;
    void *curr;
    struct cos_stk_item *stk_item;

    for(i = 0; i < MAX_NUM_SPDS; i++){
        info = &spd_stk_info_list[i];
        if(info->ci == NULL){
            continue;
        }
        printc("SPDID: %d\n", i);
        curr = (void *)info->ci->cos_stacks.freelists[0].freelist;
        if(curr == NULL){
            continue;
        }
        printc("Found curr: %X\n", curr);
        stk_item = stkmgr_get_cos_stk_item((vaddr_t)curr);
        while(stk_item){
            printc("curr: %X\n"\
                   "flags: %X\n"\
                   "next: %X\n",
                   (unsigned int)stk_item->stk,
                   (unsigned int)stk_item->stk->flags,
                   (unsigned int)stk_item->stk->next);
            print_flags(stk_item->stk);
            curr = stk_item->stk->next;
            stk_item = stkmgr_get_cos_stk_item((vaddr_t)curr);    
        }
    }

}

void
stkmgr_print_stats(void){
    int i,j;
    struct spd_stk_info *info;
    unsigned int thd_count[MAX_NUM_THREADS];
    unsigned int spd_most_active_thd;
    unsigned int spd_most_active_cnt; 
    unsigned int most_active_cnt;
    unsigned int most_active_thd;
    
    most_active_thd = 0;
    memset(thd_count, 0, MAX_NUM_THREADS);
    printc("Stack Manager Statistics\n");
    for(i = 0; i < MAX_NUM_SPDS; i++){
            
        info = &spd_stk_info_list[i];
        
        if(info->ci == NULL){
            // This means that this spdid was never mapped in
            continue; 
        }
            
        spd_most_active_thd = spd_most_active_cnt = 0;
        for(j = 0; j < MAX_NUM_THREADS; j++){
            thd_count[j] += info->thd_count[j]; 
            
            if(info->thd_count[j] > spd_most_active_cnt){
                spd_most_active_cnt = info->thd_count[j];
                spd_most_active_thd = j;
            }

            if(thd_count[j] > most_active_cnt){
                most_active_cnt = thd_count[j];
                most_active_thd = j;
            }
        }
        
        printc("SPD: %d\n"\
               "\tTotal stacks granted: %d\n"\
               "\tTotal stacks returned: %d\n"\
               "\tCurrent num allocated stacks: %d\n"\
               "\tMost Active Thread: %d Count: %d\n",
               i,
               info->num_grants,
               info->num_returns, 
               stkmgr_num_alloc_stks(i),
               spd_most_active_thd,
               spd_most_active_cnt);
    }

#ifdef PRINT_ALL_THD_COUNT
    printc("Total Thd Usage\n");
    for(i = 0; i < MAX_NUM_THREADS; i++){
        printc("Thdid: %d, count: %d\n", i, thd_count[i]);
    }
#endif
}


/**
 * This is here just to make sure we get scheduled, it can 
 * most likely be removed now
 */
void
bin(void){
    sched_block(cos_spd_id(), 0);
}

