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
#define MAX_NUM_STACKS 6 // MAX_NUM_THREADS

#define POW_2_CNT  // Should be log_2(MAX_NUM_STACKS)

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
    unsigned int stat_thd_blk[POW_2_THD];
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


static int num_blocked_thds = 0;

// List of blocked threads
struct blocked_thd blocked_thd_list;


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

/**
 * release a stack from from a given spdid
 * FIXME: I dont really know how I want to do this
 *        i.e. what should be passed, the hptr or the vaddr
 */
static inline int
stkmgr_release_stack(spdid_t s_spdid, vaddr_t s_addr){
    struct cos_stk_item *stk_item;
    mman_release_page(s_spdid, (vaddr_t)(stk_item->hptr), 0);
    return 0;
}

void
stkmgr_hello_world(void){
        printc("<stkmgr>: Hello World\n");
}

void
stkmgr_got_from_list(void){
    printc("!!!! <stkmgr> Got from local list !!!!\n");
}

void
stkmgr_return_stack(spdid_t s_spdid, void *addr){
    spdid_t spdid;
    struct cos_stk_item *stk_item;
    struct blocked_thd *bthd, *bthd_next;
    short int found = 0;
    DOUT("$$$$$: %X\n", (unsigned int)addr); 
    DOUT("Return of s_spdid is: %d from thd: %d\n", s_spdid,
    cos_get_thd_id());
   
    
    i = 0;
    // Find which component has this stack so we can unmap it
    // FIXME:  Doing a search of larger space than needed for test;
    stk_item = FIRST_LIST(&spd_stk_info_list[spdid].stk_list, next, prev);
    for(; stk_item != &spd_stk_info_list[spdid].stk_list; stk_item = stk_item->next){
        DOUT("Comparing spdid: %d,  passed addr: %X, d_addr: %X, hptr: %X\n", 
             (int)spdid,
             (unsigned int)addr,
             (unsigned int)stk_item->d_addr+4096,
             (unsigned int)stk_item->hptr);
                    
        if((unsigned int)stk_item->d_addr+4096 == addr){
            printc("Found stack item in spdid %d\n", spdid);
            found = 1;
            break;
        }
    }

    if(found != 1){
        DOUT("Unable to locate stack at address: %X\n", (unsigned int)addr);
        assert(0);
    }
    
    DOUT("Releasing Stack\n");
    mman_release_page(s_spdid, (vaddr_t)(stk_item->d_addr), 0); 
    DOUT("Putting stack back on free list\n");
    
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
    if(sched_component_take(spdid)){
        assert(0);
    }

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
    if(sched_component_release(spdid)){
        assert(0);
    }
}



struct cos_stk_item *
stkmgr_full_revoke(spdid_t *id){
    struct cos_stk_item *stk_item;
    short int found = 0;
    int i;
    // Pick a stack to revoke
    for(i = 0; i < MAX_NUM_SPDS; i++){
        stk_item = &spd_stk_info_list[i].stk_list;
        DOUT("<stkmgr>: trying to revoke from: %d\n", i); 
        if(stk_item == NULL){ 
            DOUT("<stkmgr>: stk_item is NULL\n");
            continue;
        }
        // we cant ever use the first maped in component since we
        // do not have access to the head of the list on the component
        // because that is currently being declared on the stack and is not
        // mapped between components.
        if(stk_item->next != NULL){
            found = 1;
            *id = i;
            break;
        }
    }
    if(!found){
        printc("<stkmgr>: Not enough stack available on the system for it to "\
               "run correctly\n");
        assert(0);
    }
    //stk = (struct stk_t *)(((char *)stk_item->stk) + 4096 - 8);
 
    //stk_item->stk->flags |= RELINQUISH;
    while((stk_item->stk->flags & ~IN_USE) != 0x00){
        // sleep
        // spin wait...
        ;
    }
    stk_item->stk->flags = 0;
    printc("Stack is no longer in USE!");
    //stk_item->stk->flags = 0;
    return stk_item;
}


struct cos_stk_item *
stkmgr_revoke_stack(void){
    struct cos_stk_item *stk_item, *curr, *prev, *next, *item_list_head;
    struct cos_stk_item *our_prev;
    unsigned short int stk_found;
    int i;
    int size;
    spdid_t d_spdid;
    DOUT("<stkmgr>: Attempting to revoke a stack\n");
  
    for(i = 0; i < MAX_NUM_SPDS; i++){
        item_list_head = &spd_stk_info_list[i].stk_list;
        DOUT("<stkmgr>: trying to revoke from: %d\n", i); 
        if(item_list_head == NULL){ 
            DOUT("<stkmgr>: item_list_head is NULL\n");
            continue;
        }
        // we cant ever use the first maped in component since we
        // do not have access to the head of the list on the component
        // because that is currently being declared on the stack and is not
        // mapped between components.
        if(item_list_head->next == NULL){
            DOUT("<stkmgr>: Next is NULL so list is size 1\n");
            continue;
        }
        
        DOUT("Found component with more than 1 stack available\n");
        // Here we are going to loop through all the stacks we have and see
        // if there is a stack that is not in use for us to take, there are a few
        // steps required to do this and its somewhat messy to do.  
        // Steps:
        // 1) Find a stack that is not in use
        // 2) Find the stack that points this stack by the components internal
        //    linked list (it would be nice if we had access to that, it would
        //    speed up this process but we cant).
        // 2.b) If the stack is the first in the respected components internal list
        //      we will not have access to what points to it.
        // 3) Remove the stack from the components interal linked list 
        // 4) Remove the stack from our own linked list
        // 5) Return the stack to the the component that requested it.
        // phew!!!
        stk_found = 0;
        our_prev = item_list_head;
        size = 0;
        for(stk_item = item_list_head; stk_item != NULL; stk_item = stk_item->next){
            size++;
           
            DOUT("\tspdid: stk-flags: %X stk->next: %X\n",
                   stk_item->stk->flags,
                   (unsigned int)stk_item->stk->next);
            
            /*
            printc("\tFlags: %X\tFirst Item: %X\n", 
                    stk_item->stk->flags,
                    (unsigned int)stk_item->stk->next);
            */
            if((stk_item->stk->flags & IN_USE) != 0x00){
                DOUT("\tStack In Use\n");
                continue;
            }
            
            //stk_item->stk->flags |= RELINQUISH;
            DOUT("<stmgr>: Found component %d with an avilable stack, seeing if we "\
                   "can take it back\n",
                   i);

            // We have a stack we can revoke!
            // Now we need to find what stk points to this one! 
            prev = NULL;
            next = NULL;
            for(curr = item_list_head; curr != NULL; curr = curr->next){
                //curr_stk = curr->stk; 
                //printc("\tcurr->stk->next = %p, stk_item->stk = %p\n",
                //       (void *)curr_stk->flags,
                //       (void *)stk);

                if(curr->stk->next == stk_item->stk){
                    prev = curr;
                }
                
                /*
                if(curr->stk == stk_item->stk->next){
                    next = curr;
                }
                */
                if(prev != NULL){
                    stk_found = 1;
                    DOUT("<stkmgr>: We got the info we need, relinquishing the stack!\n");
                    d_spdid = i;
                    goto stk_found;
                }
            }
            
            //stk_item->stk->flats &= ~RELINQUISH;
            our_prev = stk_item;
        }
        DOUT("Comonent: %d, size: %d\n", i, size);
   }

    // No stack found!
    //
     // do full stack take back!!!!
    DOUT("<stkmgr>: No free stacks in any component!!!!!\n" \
         "<stkmgr>: Attempting to steal from the rich and give to the poor!\n");
    stk_item = stkmgr_full_revoke(&d_spdid);
    //assert(0);
    goto ret_stack;

stk_found: 
    // At this point we should have prev set.
    // we are going to remove the stack from this components free list
    DOUT("<stkmgr>: Removing stack from components list\n");
    prev->stk->next = stk_item->stk->next; 
    
    // Now remove it from our stack list
    DOUT("<stkmgr>: Removing stack from stkmgr list\n");
    our_prev = stk_item->next;
    DOUT("<stkmgr>: Successfully revoked stack\n");

ret_stack:
    DOUT("<stkmgr>: Doing a full revoke on spdid: %d\n", d_spdid);
    
    mman_release_page(d_spdid, (vaddr_t)(stk_item->hptr), 0);
    DOUT("mman_release_page worked!\n");
    return stk_item;
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
        
    DOUT("stkmgr_request_stk_from spdid: %d\n", spdid);
    stk_item = FIRST_LIST(&spd_stk_info_list[i].stk_list, next, prev);
    for(; stk_item != &spd_stk_info_list[i].stk_list; stk_item = stk_item->next){
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
    if(sched_component_take(spdid)){
       assert(0);
    }

    bthd->thd_id = cos_get_thd_id();
    DOUT("Adding thd to the blocked list: %d\n", bthd->thd_id);
    ADD_LIST(&blocked_thd_list, bthd, next, prev);
    
    if(sched_component_release(spdid)){
        assert(0);
    } 

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
            assert(0);
        }
        DOUT("Mapped page\n");
        stk_item->d_addr = d_addr;

        // Add stack to allocated stack array
        DOUT("Adding to local spdid stk list\n");
        ADD_LIST(&spd_stk_info_list[d_spdid].stk_list, stk_item, next, prev); 
         
        info->thd_count[cos_get_thd_id()]++;
        
        DOUT("Returning Stack address: %X\n",(unsigned int)d_addr);

        return (void *)ret;
}

void
stkmgr_print_stats(void){
    struct cos_stk_item *stk_item;
    int num_invocations;
    int thd_count[MAX_NUM_THREADS];
    int i,j;
    struct spd_stk_info *info;
    int spd_most_active_thd;
    int spd_most_active_cnt; 
    int most_active_cnt;
    int most_active_thd;
    
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
            
            if(info->thd_count > spd_most_active_cnt){
                spd_most_active_cnt = info->thd_count;
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

