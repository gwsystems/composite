/**
 * Copyright 2015, by Qi Wang, interwq@gwu.edu.  All rights
 * reserved.
 *
 * Memory management using the PARSEC technique.
 */

#define COS_FMT_PRINT
#include <cos_component.h>
#include <cos_debug.h>
//#include <cos_alloc.h>
//#include <print.h>
#include <stdio.h>
//#include <string.h>

#include <cos_config.h>
#include <ertrie.h>

#include <mem_mgr.h>

int
prints(char *str)
{
	return 0;
}

int __attribute__((format(printf,1,2))) 
printc(char *fmt, ...)
{
	char s[128];
	va_list arg_ptr;
	int ret, len = 128;

	va_start(arg_ptr, fmt);
	ret = vsnprintf(s, len, fmt, arg_ptr);
	va_end(arg_ptr);
	cos_print(s, ret);

	return ret;
}

#include "../parsec.h"

static void mm_init(void);
void call(void) { 
	mm_init();
	call_cap(0,0,0,0,0);
	return; 
}

#include <ck_pr.h>
//#include <cos_list.h>
//#include "../../sched/cos_sched_ds.h"
//#include "../../sched/cos_sched_sync.h"

#include <ck_spinlock.h>
#if NUM_CPU_COS > 1
/* ck_spinlock_ticket_t xcore_lock = CK_SPINLOCK_TICKET_INITIALIZER; */

/* #define LOCK()   do { if (cos_sched_lock_take())   assert(0); ck_spinlock_ticket_lock_pb(&xcore_lock, 1); } while (0) */
/* #define UNLOCK() do { ck_spinlock_ticket_unlock(&xcore_lock); if (cos_sched_lock_release()) assert(0);    } while (0) */
/* #else */
/* #define LOCK()   if (cos_sched_lock_take())    assert(0); */
/* #define UNLOCK() if (cos_sched_lock_release()) assert(0); */
#endif

/***************************************************/
/*** Data-structure for tracking physical memory ***/
/***************************************************/

struct mapping {
	int flags;
	vaddr_t addr;
	/* siblings and children */
	struct mapping *sibling_next, *sibling_prev;
	struct mapping *parent, *child;
};
typedef struct mapping mapping_t ;

struct frame {
	/* frame id or paddr here? */
	paddr_t paddr;
	struct mapping *child;
	ck_spinlock_ticket_t frame_lock;
};
typedef struct frame frame_t ;

parsec_ns_t comp CACHE_ALIGNED;
parsec_ns_t frame_ns CACHE_ALIGNED;
parsec_ns_t kmem_ns CACHE_ALIGNED;
parsec_ns_t kern_frames CACHE_ALIGNED;
parsec_t mm CACHE_ALIGNED;

#define PGTBL_PAGEIDX_SHIFT (12)
#define PGTBL_FRAME_BITS    (32 - PGTBL_PAGEIDX_SHIFT)
#define PGTBL_FLAG_MASK     ((1<<PGTBL_PAGEIDX_SHIFT)-1)
#define PGTBL_FRAME_MASK    (~PGTBL_FLAG_MASK)
#define PGTBL_DEPTH         2
#define PGTBL_ORD           10
#define MAPPING_TBL_ORD     7

static int __mapping_isnull(struct ert_intern *a, void *accum, int isleaf) 
{ (void)isleaf; (void)accum; return !(u32_t)(a->next); }

//
//	       addr >> PGTBL_PAGEIDX_SHIFT, PGTBL_DEPTH, flags);

// we use the ert lookup function only 
ERT_CREATE(__mappingtbl, mappingtbl, PGTBL_DEPTH, PGTBL_ORD, sizeof(int*), MAPPING_TBL_ORD, sizeof(struct mapping), NULL, \
	   NULL, NULL, __mapping_isnull, NULL,	\
	   NULL, NULL, NULL, ert_defresolve);
typedef struct mappingtbl * mappingtbl_t; 

static mapping_t * 
mapping_lookup(mappingtbl_t tbl, vaddr_t addr) 
{
	unsigned long flags;
	return __mappingtbl_lkupan(tbl, addr, PGTBL_DEPTH, &flags);
	/* return __pgtbl_lkupan((pgtbl_t)((unsigned long)pt | PGTBL_PRESENT),  */
	/* 		      addr >> PGTBL_PAGEIDX_SHIFT, PGTBL_DEPTH, flags); */
}

struct comp {
	parsec_ns_t mapping_ns CACHE_ALIGNED;
	int id;
	ck_spinlock_ticket_t comp_lock;
};

#define NREGIONS 4
extern struct cos_component_information cos_comp_info;

/***************************************************/
/*** ... ***/
/***************************************************/

char frame_table[CACHE_LINE*COS_MAX_MEMORY] PAGE_ALIGNED;
char kmem_table[CACHE_LINE*COS_KERNEL_MEMORY] PAGE_ALIGNED;

struct frame *
frame_lookup(unsigned long id, parsec_ns_t *ns) 
{
	struct quie_mem_meta *meta;
	
	meta = (struct quie_mem_meta *)((void *)(ns->tbl) + id * CACHE_LINE);
	if (ACCESS_ONCE(meta->flags) & PARSEC_FLAG_DEACT) return 0;

	return (struct frame *)((char *)meta + sizeof(struct quie_mem_meta));
}

#define TLB_QUIE_PERIOD (TLB_QUIESCENCE_CYCLES)
/* return 0 if quiesced */
static int 
tlb_quiesce(quie_time_t t, int waiting)
{
	quie_time_t curr = get_time();
	
	if (unlikely(curr <= t)) return -EINVAL;

	do {
		if (curr - t > TLB_QUIE_PERIOD) return 0;
		curr = get_time();
	} while (waiting);
	
	return -1;
}

static int
frame_quiesce(quie_time_t t, int waiting)
{
	tlb_quiesce(t, waiting);
	/* make sure lib quiesced as well -- very likely already. */
	parsec_sync_quiescence(t, waiting, &mm);

	return 0;
}

static int 
frame_free(frame_t *f) 
{
	return parsec_free(f, &(frame_ns.allocator));
}

struct frame *
frame_alloc(void)
{
	/* The size (1st argument) is meaningless here. We are
	 * allocating a page. */
	return parsec_alloc(0, &(frame_ns.allocator), 1);
}

static void 
frame_init(void)
{
	int ret, i, n_frames;
	vaddr_t frame_addr = BOOT_MEM_PM_BASE;
	frame_t *frame;

	assert(CACHE_LINE >= (sizeof(struct quie_mem_meta) + sizeof(struct frame)));
	assert(frame_addr % PAGE_SIZE == 0);

	memset((void *)frame_table, 0, CACHE_LINE*COS_MAX_MEMORY);
	frame_ns.tbl     = (void *)frame_table;
	frame_ns.lookup  = frame_lookup;
	frame_ns.alloc   = frame_alloc;
	frame_ns.free    = frame_free;
	frame_ns.allocator.quiesce = frame_quiesce;

	/* Detecting all the frames. */
	i = 0;
	while (1) {
		ret = call_cap_op(MM_CAPTBL_OWN_PGTBL, CAPTBL_OP_INTROSPECT, frame_addr, 0,0,0);
		frame = frame_lookup(i, &frame_ns);

		if (!ret || !frame) break;

		frame->paddr = ret & PAGE_MASK;
		ck_spinlock_ticket_init(&(frame->frame_lock));
		frame_addr += PAGE_SIZE;
		i++;
	}

	n_frames = i;
	/* Quiescence-waiting queue setting. */
	frame_ns.allocator.qwq_min_limit = 64;
	frame_ns.allocator.qwq_max_limit = 64 * 4;
	frame_ns.allocator.n_local       = NUM_CPU;

	for (i = 0; i < n_frames; i++) {
		/* They'll be on the glb freelist. */
		frame = frame_lookup(i, &frame_ns);
//		printc("%d frame %x\n", i, frame->paddr);
		ret = glb_freelist_add(frame, &(frame_ns.allocator));
		assert(ret == 0);
	}

	printc("Mem_mgr: initialized %d physical frames\n", n_frames);

	///////////////////
	/* struct frame *new[64]; */
	/* for (i = 0; i < 64; i++) { */
	/* 	new[i] = frame_alloc(); */
	/* 	if (new[i]) { */
	/* 		printc("%d: new @ %p, paddr %x\n", i, new[i], new[i]->paddr); */
	/* 		frame_free(new[i]); */
	/* 	} else printc("got nothing %d...\n", i); */
	/* } */

	/* new[0] = frame_alloc(); */
	/* if (new[0])  */
	/* 	printc("last new @ %p, paddr %x\n", new[0], new[0]->paddr); */
	/* else  */
	/* 	printc("got nothing %d...\n", i); */

	return;
}

static void 
kmem_init(void)
{
	int ret, i, n_frames;
	vaddr_t kmem_addr = BOOT_MEM_KM_BASE;
	frame_t *kmem;

	assert(CACHE_LINE >= (sizeof(struct quie_mem_meta) + sizeof(struct frame)));
	assert(kmem_addr % PAGE_SIZE == 0);

	memset((void *)kmem_table, 0, CACHE_LINE*COS_KERNEL_MEMORY);

	kmem_ns.tbl     = (void *)kmem_table;
	kmem_ns.lookup  = frame_lookup;
	kmem_ns.alloc   = frame_alloc;
	kmem_ns.free    = frame_free;
	kmem_ns.allocator.quiesce = frame_quiesce;

	/* Detecting all the frames. */
	i = 0;
	while (1) {
		ret = call_cap_op(MM_CAPTBL_OWN_PGTBL, CAPTBL_OP_INTROSPECT, kmem_addr, 0,0,0);
		kmem = frame_lookup(i, &kmem_ns);

		if (!ret || !kmem) break;

		kmem->paddr = ret & PAGE_MASK;
		ck_spinlock_ticket_init(&(kmem->frame_lock));
		kmem_addr += PAGE_SIZE;
		i++;
	}

	n_frames = i;
	/* Quiescence-waiting queue setting. */
	kmem_ns.allocator.qwq_min_limit = 64;
	kmem_ns.allocator.qwq_max_limit = 64 * 4;
	kmem_ns.allocator.n_local       = NUM_CPU;

	for (i = 0; i < n_frames; i++) {
		/* They'll be on the glb freelist. */
		kmem = frame_lookup(i, &kmem_ns);
		ret = glb_freelist_add(kmem, &(kmem_ns.allocator));
		assert(ret == 0);
	}

	printc("Mem_mgr: initialized %d kernel frames.\n", n_frames);

	return;
}

static void
mm_init(void)
{
	printc("In mm init, %d %d, comp %ld, thd %d, cpu %ld\n", sizeof(struct mapping), (PAGE_SIZE/(1<<MAPPING_TBL_ORD)),
	       cos_spd_id(), cos_get_thd_id(), cos_cpuid());

	if (sizeof(struct mapping) > (PAGE_SIZE/(1<<MAPPING_TBL_ORD))) {
		printc("MM init: MAPPING TBL SIZE / ORD error!\n");
		BUG();
	}

	parsec_init(&mm);

	frame_init();
	kmem_init();

	/* printc("core %ld: mm init as thread %d\n", cos_cpuid(), cos_get_thd_id()); */

	/* /\* Expanding VAS. *\/ */
	/* printc("mm expanding %lu MBs @ %p\n", (NREGIONS-1) * round_up_to_pgd_page(1) / 1024 / 1024,  */
	/*        (void *)round_up_to_pgd_page((unsigned long)&cos_comp_info.cos_poly[1])); */
	/* if (cos_vas_cntl(COS_VAS_SPD_EXPAND, cos_spd_id(),  */
	/* 		 round_up_to_pgd_page((unsigned long)&cos_comp_info.cos_poly[1]),  */
	/* 		 (NREGIONS-1) * round_up_to_pgd_page(1))) { */
	/* 	printc("MM could not expand VAS\n"); */
	/* 	BUG(); */
	/* } */

	/* frame_init(); */

	/* printc("core %ld: mm init done\n", cos_cpuid()); */
}

/**************************/
/*** Mapping operations ***/
/**************************/
/**********************************/
/*** Public interface functions ***/
/**********************************/

vaddr_t mman_get_page(spdid_t spd, vaddr_t addr, int flags)
{
	printc("in get page\n");
	call_cap(0,0,0,0,0);

	return 0;
}

vaddr_t __mman_alias_page(spdid_t s_spd, vaddr_t s_addr, u32_t d_spd_flags, vaddr_t d_addr)
{
	printc("in alias page\n");
	call_cap(0,0,0,0,0);


	return 0;
}

int mman_revoke_page(spdid_t spd, vaddr_t addr, int flags)
{
	printc("in revoke page\n");
	call_cap(0,0,0,0,0);

	return 0;
}

int mman_release_page(spdid_t spd, vaddr_t addr, int flags)
{
	printc("in release page\n");
	call_cap(0,0,0,0,0);

	return 0;
}

void mman_print_stats(void) {}

void mman_release_all(void) {}

PERCPU_ATTR(static volatile, int, initialized_core); /* record the cores that still depend on us */

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	printc("upcall in mm, %ld %d\n", cos_cpuid(), cos_get_thd_id());
	/* printc("cpu %ld: thd %d in mem_mgr init. args %d, %p, %p, %p\n", */
	/*        cos_cpuid(), cos_get_thd_id(), t, arg1, arg2, arg3); */
	switch (t) {
	case COS_UPCALL_THD_CREATE:
		if (cos_cpuid() == INIT_CORE) {
			int i;
			for (i = 0; i < NUM_CPU; i++)
				*PERCPU_GET_TARGET(initialized_core, i) = 0;
			mm_init(); 
		} else {
			/* Make sure that the initializing core does
			 * the initialization before any other core
			 * progresses */
			while (*PERCPU_GET_TARGET(initialized_core, INIT_CORE) == 0) ;
		}
		*PERCPU_GET(initialized_core) = 1;
		break;			
	default:
		BUG(); return;
	}

	return;
}
