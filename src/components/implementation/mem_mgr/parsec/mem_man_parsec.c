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
#define ERT_DEBUG
#include "ertrie.h"

#include <mem_mgr.h>

unsigned long  n_ops = 0;
unsigned long long tot = 0;

int
prints(char *s)
{
	int len = strlen(s);
	cos_print(s, len);
	return len;
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
	return;
}

#include <ck_pr.h>
#include <ck_spinlock.h>

/***************************************************/
/*** Data-structure for tracking physical memory ***/
/***************************************************/

enum {
	MM_MAPPING_ACTIVE = 1,
};

struct mapping {
	int flags;
	vaddr_t vaddr;
	unsigned long frame_id;
	/* siblings and children */
	struct mapping *sibling_next, *sibling_prev;
	struct mapping *parent, *child;
	ck_spinlock_ticket_t m_lock;
};
typedef struct mapping mapping_t ;

struct frame {
	/* We don't need the actual physical address of the
	 * frame. The capability to it is sufficient. */
	vaddr_t cap;
	unsigned long id;
	struct mapping *child;
	ck_spinlock_ticket_t frame_lock;
};
typedef struct frame frame_t ;

struct comp {
	parsec_ns_t mapping_ns;
	int id;
	ck_spinlock_ticket_t comp_lock; /* only used for init */
};
typedef struct comp comp_t ;

static int mapping_trylock(mapping_t *m) {
	return ck_spinlock_ticket_trylock(&m->m_lock);
}

static void mapping_lock(mapping_t *m) {
	if (mapping_trylock(m)) return;

	printc("mapping lock contention??\n");
	ck_spinlock_ticket_lock(&m->m_lock);
}

static void mapping_unlock(mapping_t *m) {
	ck_spinlock_ticket_unlock(&m->m_lock);
}

static int frame_trylock(frame_t *f) {
	return ck_spinlock_ticket_trylock(&f->frame_lock);
}

static void frame_lock(frame_t *f) {
	if (frame_trylock(f)) return;

	printc("frame lock contention??\n");
	ck_spinlock_ticket_lock(&f->frame_lock);
}

static void frame_unlock(frame_t *f) {
	ck_spinlock_ticket_unlock(&f->frame_lock);
}

static void comp_lock(comp_t *c) {
	ck_spinlock_ticket_lock(&c->comp_lock);
}

static void comp_unlock(comp_t *c) {
	ck_spinlock_ticket_unlock(&c->comp_lock);
}

parsec_ns_t comp_ns     CACHE_ALIGNED;
parsec_ns_t frame_ns    CACHE_ALIGNED;
parsec_ns_t kmem_ns     CACHE_ALIGNED;

#define FRAMETBL_ITEM_SZ (CACHE_LINE)
/* Set on init, never modify. */
static unsigned long n_pmem, n_kmem;
char frame_table[FRAMETBL_ITEM_SZ*COS_MAX_MEMORY + 1]   CACHE_ALIGNED;
char kmem_table[FRAMETBL_ITEM_SZ*COS_KERNEL_MEMORY + 1] CACHE_ALIGNED;

//#define COMP_ITEM_SZ     (CACHE_LINE)
#define COMP_ITEM_SZ (sizeof(struct comp) + 2*CACHE_LINE - sizeof(struct comp) % CACHE_LINE)

char comp_table[COMP_ITEM_SZ*MAX_NUM_COMPS] CACHE_ALIGNED;

/* QWQ -- Quiescence-Waiting Queue */
#define VAS_QWQ_SIZE        (1*1024)//(1024)
#define VAS_QWQ_SIZE_SMALL  (1)	/* For large VAS items, to avoid queuing too much. */

#define PMEM_QWQ_SIZE (1024)  /* 4 MB */
#define KMEM_QWQ_SIZE (1) /* Kmem allocation much less often. */

/* All namespaces in the same parallel section. */
parsec_t mm CACHE_ALIGNED;

#define PGTBL_PAGEIDX_SHIFT (12)
#define PGTBL_FRAME_BITS    (32 - PGTBL_PAGEIDX_SHIFT)
#define PGTBL_FLAG_MASK     ((1<<PGTBL_PAGEIDX_SHIFT)-1)
#define PGTBL_FRAME_MASK    (~PGTBL_FLAG_MASK)
#define PGTBL_DEPTH         2
#define PGTBL_ORD           10

/* Mem_mgr itself only allocates 2 sizes: pgd size(page size), and pte
 * size (larger because of the mapping info). We pre-allocate VAS
 * table for the MM -- as it'd be hard to apply the expand function to
 * the VAS of MM itself. */

#define MAPPING_ITEM_SZ CACHE_LINE
char mm_own_pgd[PAGE_SIZE] PAGE_ALIGNED;

#define MM_PGD_NPAGES 1
#define MM_PTE_NPAGES (MAPPING_ITEM_SZ/sizeof(void *))
#define MM_PTE_SIZE   (PAGE_SIZE * MM_PTE_NPAGES)

#define NPTE_ENTRY_PER_PGD (PAGE_SIZE/sizeof(void *))
/* How many PTEs (or to say, VAS) needed in the MM to manage all the
 * __PTEs__ for other components. */
#define MIN_N_PTE (COS_MAX_MEMORY/NPTE_ENTRY_PER_PGD / (NPTE_ENTRY_PER_PGD/MM_PTE_NPAGES) + 1)
/* this is pessimistic. just to be safe.  */
#define MM_NPTE_NEEDED 60//(MIN_N_PTE*40)

/* VAS management of the MM. */
/* MM local memory used for PGDs. */
char mm_vas_pgd[MM_PTE_SIZE] PAGE_ALIGNED;
/* and this one is for PTEs */
char mm_vas_pte[MM_PTE_SIZE*MM_NPTE_NEEDED] PAGE_ALIGNED;

// we use the ert lookup function only
ERT_CREATE(__mappingtbl, mappingtbl, PGTBL_DEPTH, PGTBL_ORD, sizeof(int*), PGTBL_ORD, MAPPING_ITEM_SZ, NULL, \
	   NULL, ert_defget, ert_defisnull, NULL,	\
	   NULL, NULL, NULL, ert_defresolve);
typedef struct mappingtbl * mappingtbl_t;

static mapping_t *
mapping_lookup(comp_t *c, vaddr_t addr)
{
	unsigned long flags;
	struct quie_mem_meta *meta;

	meta = (struct quie_mem_meta *)__mappingtbl_lkupan((mappingtbl_t)c->mapping_ns.tbl,
							   addr >> PGTBL_PAGEIDX_SHIFT, PGTBL_DEPTH, &flags);

	if (!meta) return NULL;
	if (meta->flags & PARSEC_FLAG_DEACT) return NULL;

	return (void *)meta + sizeof(struct quie_mem_meta);
}

#define NREGIONS 4
extern struct cos_component_information cos_comp_info;

/***************************************************/
/*** ... ***/
/***************************************************/

struct frame *
frame_lookup(unsigned long id, parsec_ns_t *ns)
{
	struct quie_mem_meta *meta;

	meta = (struct quie_mem_meta *)((void *)(ns->tbl) + id * FRAMETBL_ITEM_SZ);
	if (ACCESS_ONCE(meta->flags) & PARSEC_FLAG_DEACT) return 0;

	return (struct frame *)((char *)meta + sizeof(struct quie_mem_meta));
}

/* i tried using a per-core last_tlb_flush for tlb quiescence. Did not
 * help. A simple glb_flush time + lock works better. */
struct tlb_flush {
	volatile quie_time_t last_tlb_flush;
	volatile unsigned long glb_tlb_flush;
	ck_spinlock_mcs_t lock;
	char __padding[CACHE_LINE*2 - sizeof(quie_time_t) - sizeof(unsigned long) - sizeof(ck_spinlock_mcs_t)];
};
struct tlb_flush tlb_flush CACHE_ALIGNED;

#define TLB_QUIE_PERIOD (TLB_QUIESCENCE_CYCLES)

/* return 0 if quiesced. */
static int
check_tlb_quiesce(quie_time_t t)
{
	quie_time_t curr;

	if (t < tlb_flush.last_tlb_flush) return 0;
	curr = get_time();
	if (curr < t) return -1;
	if (curr - t > TLB_QUIE_PERIOD) return 0;

	return -1;
}

static int
tlb_quiesce(quie_time_t t)
{
	int cpu, i, target_cpu;
	quie_time_t curr;
	ck_spinlock_mcs_context_t mcs;

	if (check_tlb_quiesce(t) == 0) return 0;

	/* Slow path below: on demand per-core TLB flush. */
	ck_spinlock_mcs_lock(&(tlb_flush.lock), &mcs);

	/* tlb_flush.glb_tlb_flush++; */
	/* cos_mem_fence(); */

	/* re-check after took the lock */
	if (check_tlb_quiesce(t) == 0) goto done;

	cpu = cos_cpuid();
	curr = get_time();
	/* We need an explicit TLB flush. */
	for (i = 0; i < NUM_CPU_COS; i++) {
		target_cpu = (cpu+i) % NUM_CPU_COS;
		printc("FLUSH!%c", target_cpu); /* a hack for now */
	}
	/* commit */
	tlb_flush.last_tlb_flush = curr;
done:
	ck_spinlock_mcs_unlock(&(tlb_flush.lock), &mcs);

	return 0;
}

static int
frame_quiesce(quie_time_t t, int waiting)
{
	tlb_quiesce(t);
	/* make sure lib quiesced as well -- very likely already. */
	parsec_sync_quiescence(t, waiting, &mm);

	return 0;
}

static int
pmem_free(frame_t *f)
{
	if (unlikely(!((void *)f >= frame_ns.tbl && (void *)f <= (frame_ns.tbl + FRAMETBL_ITEM_SZ*n_pmem)))) {
		printc("Freeing unknown physical frame %p\n", (void *)f);
		return -EINVAL;
	}

	return parsec_free(f, &(frame_ns.allocator));
}

static int
kmem_free(frame_t *f)
{
	if (unlikely(!((void *)f >= kmem_ns.tbl && (void *)f <= (kmem_ns.tbl + FRAMETBL_ITEM_SZ*n_kmem)))) {
		printc("Freeing unknown kernel frame %p\n", (void *)f);
		return -EINVAL;
	}

	return parsec_free(f, &(kmem_ns.allocator));
}

/* This detects the namespace automatically. */
static int
frame_free(frame_t *f_addr)
{
	void *f = (void *)f_addr;

	if (f >= frame_ns.tbl && f <= (frame_ns.tbl + FRAMETBL_ITEM_SZ*n_pmem))
		return parsec_free(f, &(frame_ns.allocator));

	if (unlikely(!(f >= kmem_ns.tbl && f <= (kmem_ns.tbl + FRAMETBL_ITEM_SZ*n_kmem)))) {
		printc("Freeing unknown frame %p ktbl %p, ptbl %p\n", f, kmem_ns.tbl, frame_ns.tbl);
		return -EINVAL;
	}

	return parsec_free(f, &(kmem_ns.allocator));
}

static struct frame *
frame_alloc(parsec_ns_t *ns)
{
	 frame_t *f = parsec_desc_alloc(PAGE_SIZE, &(ns->allocator), 1);

	 return f;
}

static struct frame *
get_pmem(void)
{
	frame_t *f = frame_alloc(&frame_ns);

	return f;
}

static struct frame *
get_kmem(void)
{
	frame_t *kmem;

	kmem = frame_alloc(&kmem_ns);
	if (!kmem) {
		printc("MM: no enough kernel frames on core %ld\n", cos_cpuid());
	}

	return kmem;
}

/* Still maintain single global address space, so that we can
 * implement Mutable Protection Domain (MPD) if needed. */
#define VAS_UNIT (1 << (PGTBL_PAGEIDX_SHIFT + PGTBL_ORD)) //@ 4MB
#define COS_FREE_VAS (BOOT_MEM_VM_BASE + VAS_UNIT*MAX_NUM_COMPS) //get this from booter!

vaddr_t free_vas = COS_FREE_VAS;

static vaddr_t mm_local_get_page(unsigned long n_pages);

int
mappingtbl_cons(mappingtbl_t tbl, vaddr_t expand_addr, vaddr_t pte)
{
	unsigned long *intern;
	unsigned long old_v, flags;
	int ret;

	intern = __mappingtbl_lkupani(tbl, expand_addr >> PGTBL_PAGEIDX_SHIFT, 0, 1, &flags);

	if (!intern) return -ENOENT;
	if (*intern) return -EPERM;

	ret = cos_cas(intern, 0, pte);
	if (ret != CAS_SUCCESS) return -ECASFAIL;

	return 0;
}

/* this probably should be managed by a separate manager. */
static vaddr_t
vas_region_alloc(void)
{
	vaddr_t new, vas;

	do {
		vas = free_vas;
		new = vas + VAS_UNIT;
	} while (cos_cas(&free_vas, vas, new) != CAS_SUCCESS) ;

	return vas;
}

static capid_t
comp_pt_cap(int comp)
{
	return comp*captbl_idsize(CAP_COMP) + MM_CAPTBL_OWN_PGTBL;
}

static unsigned long mm_alloc_pte_cap(void);

static int pgtbl_act(comp_t *comp, vaddr_t pte)
{
	vaddr_t pte_kmem;
	frame_t *kmem;
	capid_t pte_cap;
	int max_try = 10;

	kmem = get_kmem();
	if (!kmem) {
		return -1;
	}

	pte_kmem = kmem->cap;
	pte_cap  = mm_alloc_pte_cap();

	/* TODO: error handling. */
	while (call_cap_op(MM_CAPTBL_OWN_CAPTBL, CAPTBL_OP_PGTBLACTIVATE,
			   pte_cap, MM_CAPTBL_OWN_PGTBL, pte_kmem, 1)) {
		max_try--;
		if (!max_try) {
			printc("Expanding VAS of comp %d: Fail to act cap @ %d, using kmem %x\n",
			       comp->id, (int)pte_cap, (unsigned int)pte_kmem);
			return -1;
		}
	}
	/* Connect PTE to the component's pgtbl */
	if (call_cap_op(comp_pt_cap(comp->id), CAPTBL_OP_CONS, pte_cap, pte, 0, 0)) {
		printc("Expanding VAS of comp %d: fail to cons pgtbl @ %x\n", comp->id, (unsigned int)pte);
		return -1;
	}

	return 0;
}

static int
comp_vas_region_alloc(comp_t *comp, vaddr_t local_tbl, unsigned long vas_unit_sz,
		      int (*add_fn)(void *, struct parsec_allocator *))
{
	int n_items, i, j, ret;
	vaddr_t mapping_vaddr, pte;
	mapping_t *m, *temp_m;
	struct quie_mem_meta *meta;
	parsec_ns_t *ns;

	ns = &comp->mapping_ns;

	pte = vas_region_alloc();

	/* TODO: error handling. */
	ret = pgtbl_act(comp, pte);
	if (ret) return -1;

	ret = mappingtbl_cons((mappingtbl_t)ns->tbl, pte, (vaddr_t)local_tbl);
	if (ret) return -1;

	/* # of free vas items in this one pte. */
	n_items = NPTE_ENTRY_PER_PGD / (vas_unit_sz/PAGE_SIZE);

	mapping_vaddr = pte;
	for (i = 0; i < n_items; i++) {
		m = mapping_lookup(comp, mapping_vaddr);
		if (!m) {
			printc("ERROR: no entry in new pte %x\n", (unsigned int)mapping_vaddr);
			return -1;
		}

		temp_m = m;
		for (j = 0; j < (int)(vas_unit_sz / PAGE_SIZE); j++) {
			memset(temp_m, 0, sizeof(struct mapping));
			meta = (void *)temp_m - sizeof(struct quie_mem_meta);
			meta->flags |= PARSEC_FLAG_DEACT;
			meta->size = 0;

			temp_m->vaddr = mapping_vaddr;

			mapping_vaddr += PAGE_SIZE;
			temp_m = (void *)temp_m + MAPPING_ITEM_SZ;
		}

		/* set size so it goes to the corresponding slab. */
		meta = (void *)m - sizeof(struct quie_mem_meta);
		meta->size = vas_unit_sz;

		/* printc("comp %d add vas %x, size %d\n", comp->id, m->vaddr, meta->size); */
		add_fn(m, &(ns->allocator));
	}
	/* printc("cpu %d expanded and got %d new items.\n", cos_cpuid(), n_items); */

	return 0;
}

static int
vas_expand(comp_t *comp, unsigned long unit_size)
{
	int ret;
	vaddr_t pte;

	/* printc("cpu %d: vas expand for comp %d\n", cos_cpuid(), comp->id); */
	assert(comp);
	pte = mm_local_get_page(MM_PTE_NPAGES);
	if (!pte) {
		printc("Mem_mgr CPU %ld: error -- no enough local VAS\n", cos_cpuid());
		return -ENOMEM;
	}
	/* printc("comp %d expand for unit_size %d\n", comp->id, unit_size); */

	return comp_vas_region_alloc(comp, pte, unit_size, qwq_add_freeitem);
}

static void
mapping_init(mapping_t *m)
{
	struct quie_mem_meta *meta;

	mapping_lock(m);

	m->flags = 0;
	m->frame_id = 0;
	m->sibling_next = m->sibling_prev = NULL;
	m->parent = m->child = NULL;

	parsec_desc_activate(m);

	mapping_unlock(m);
}

static inline int
mapping_close(mapping_t *m)
{
	return parsec_desc_deact(m);
}

static struct mapping *
vas_alloc(parsec_ns_t *ns, unsigned long size)
{
	int n_page, i, ret;
	mapping_t *new_mapping, *m;

	n_page = size / PAGE_SIZE;
	assert(size % PAGE_SIZE == 0);

	new_mapping = parsec_desc_alloc(size, &(ns->allocator), 1);

	if (!new_mapping && ns->expand) {
		int (*expand_fn)(parsec_ns_t *, unsigned long) = ns->expand;
		int n_try = 10;
		assert(expand_fn);
		while (n_try-- && !new_mapping) {
			ret = expand_fn(ns, size);
			if (ret) printc("CPU %ld: expanding failed!\n", cos_cpuid());
			new_mapping = parsec_desc_alloc(size, &(ns->allocator), 1);
		}
	}

	if (new_mapping) {
		m = new_mapping;
		for (i = 0; i < n_page; i++) {
			mapping_init(m);
			m = (void *)m + MAPPING_ITEM_SZ;
		}
	}

	return new_mapping;
}

static int
vas_free(mapping_t *m, parsec_ns_t *ns)
{
	int ret;
	/* Size == 0 means it shouldn't go back to the freelist. The
	 * mapping is a page in a larger allocation. */
	if (parsec_item_size(m) == 0) return mapping_close(m);

	ret = parsec_free(m, &(ns->allocator));

	return ret;
}

static unsigned long
frame_boot(vaddr_t frame_addr, parsec_ns_t *ns)
{
	int ret;
	unsigned long n_frames, i;
	frame_t *frame;
	struct quie_mem_meta *meta;

	assert(FRAMETBL_ITEM_SZ >= (sizeof(struct quie_mem_meta) + sizeof(struct frame)));
	assert(frame_addr % PAGE_SIZE == 0);

	/* Frame id 0 means no physical frame mapped. Don't use id
	 * 0 for real frames. */
	i = 1;
	while (1) {
		ret = call_cap_op(MM_CAPTBL_OWN_PGTBL, CAPTBL_OP_INTROSPECT, frame_addr, 0,0,0);
		frame = frame_lookup(i, ns);

		if (!ret || !frame) break;

		frame->cap = frame_addr;
		frame->id  = i;
		ck_spinlock_ticket_init(&(frame->frame_lock));

		meta = (void *)frame - sizeof(struct quie_mem_meta);
		meta->size = PAGE_SIZE;

		frame_addr += PAGE_SIZE;
		i++;
	}

	n_frames = i - 1;

	for (i = 0; i < n_frames; i++) {
		/* They'll added to the glb freelist. */
		frame = frame_lookup(i, ns);
		ret = glb_freelist_add(frame, &(ns->allocator));
		assert(ret == 0);
	}

	return n_frames;
}

static void
frame_init(void)
{
	int i, j;

	memset(&frame_ns, 0, sizeof(struct parsec_ns));
	memset((void *)frame_table, 0, FRAMETBL_ITEM_SZ*COS_MAX_MEMORY);

	frame_ns.tbl     = (void *)frame_table;
	frame_ns.item_sz = FRAMETBL_ITEM_SZ;
	frame_ns.lookup  = frame_lookup;
	frame_ns.alloc   = frame_alloc;
	frame_ns.free    = frame_free;
	frame_ns.allocator.quiesce = frame_quiesce;

	/* Quiescence-waiting queue setting. */
	frame_ns.allocator.qwq_min_limit = PMEM_QWQ_SIZE;
	frame_ns.allocator.qwq_max_limit = (unsigned long)(-1); //PMEM_QWQ_SIZE * 4;
	frame_ns.allocator.n_local       = NUM_CPU;

	frame_ns.parsec = &mm;

	for (i = 0; i < NUM_CPU; i++) {
		for (j = 0; j < QUIE_QUEUE_N_SLAB; j++) {
			frame_ns.allocator.qwq[i].slab_queue[j].qwq_min_limit = PMEM_QWQ_SIZE;
		}
	}

	/* Detecting all the frames. */
	n_pmem = frame_boot(BOOT_MEM_KM_BASE, &frame_ns);

	return;
}

static void
kmem_init(void)
{
	int i, j;

	memset(&kmem_ns, 0, sizeof(struct parsec_ns));
	memset((void *)kmem_table, 0, FRAMETBL_ITEM_SZ*COS_KERNEL_MEMORY);

	kmem_ns.tbl     = (void *)kmem_table;
	kmem_ns.item_sz = FRAMETBL_ITEM_SZ;
	kmem_ns.lookup  = frame_lookup;
	kmem_ns.alloc   = frame_alloc;
	kmem_ns.free    = frame_free;
	kmem_ns.allocator.quiesce = frame_quiesce;

	/* Quiescence-waiting queue setting. */
	kmem_ns.allocator.qwq_min_limit = KMEM_QWQ_SIZE;
	kmem_ns.allocator.qwq_max_limit = KMEM_QWQ_SIZE * 4;
	kmem_ns.allocator.n_local       = NUM_CPU;

	kmem_ns.parsec = &mm;

	for (i = 0; i < NUM_CPU; i++) {
		for (j = 0; j < QUIE_QUEUE_N_SLAB; j++) {
			kmem_ns.allocator.qwq[i].slab_queue[j].qwq_min_limit = KMEM_QWQ_SIZE;
		}
	}

	n_kmem = frame_boot(BOOT_MEM_KM_BASE, &kmem_ns);

	return;
}

struct comp *
comp_lookup(int id)
{
	struct comp *comp;

	if (unlikely(id < 0 || id >= MAX_NUM_COMPS)) return NULL;

	comp = (struct comp *)((void *)comp_table + COMP_ITEM_SZ * id);

	return comp;
}

static comp_t *mm_comp;

static int
build_mapping(comp_t *comp, frame_t *frame, mapping_t *mapping)
{
	int ret = -EINVAL;

	/* We should have unused frame and mapping here. So there
	 * should be no contention. */
	ret = frame_trylock(frame);
	if (!ret) return -EINVAL;
	mapping_lock(mapping);

	/* we have the frame & mapping lock now */
	if (!parsec_item_active(mapping) || !parsec_item_active(frame)) cos_throw(done, -EINVAL);
	if (frame->child || mapping->frame_id)  cos_throw(done, -EINVAL);

	/* and build the actual mapping */
	ret = call_cap_op(MM_CAPTBL_OWN_PGTBL, CAPTBL_OP_MEMACTIVATE,
			  frame->cap, comp_pt_cap(comp->id), mapping->vaddr, 0);
	if (ret) {
		struct quie_mem_meta *meta = (void *)mapping - sizeof(struct quie_mem_meta);
		printc("MM mapping to comp %d @ %p failed: kern ret %d, user deact %llu, curr %llu\n",
		       comp->id, (void *)(mapping->vaddr), ret, meta->time_deact, get_time());
		cos_throw(done, -EINVAL);
	}

	frame->child = mapping;
	/* This should not fail as we are using newly allocated vas. */
	if (cos_cas(&mapping->frame_id, 0, frame->id) != CAS_SUCCESS) cos_throw(done, -ECASFAIL);

	ret = 0;
done:
	mapping_unlock(mapping);
	frame_unlock(frame);

	return ret;
}

static int mapping_free(mapping_t *m, comp_t *c);

/* alloc n pages, and map them. */
static int
alloc_map_pages(mapping_t *head, comp_t *comp, int n_pages)
{
	int i, ret;
	frame_t *f;
	mapping_t *m;

	m = head;
	/* FIXME: reverse and free when fail */
	for (i = 0; i < n_pages; i++) {
		f = get_pmem();

		if (!f) {
			i--;
			printc("MM warning: out of physical memory!\n");
			cos_throw(release, -ENOMEM);
		}

		if (build_mapping(comp, f, m)) {
			struct quie_mem_meta *meta = (void *)m - sizeof(struct quie_mem_meta);
			printc("MM on core %ld error: mapping failed, t %llu!\n", cos_cpuid(), meta->time_deact);

			cos_throw(release, -EINVAL);
		}
		m = (void *)m + MAPPING_ITEM_SZ;
	}

	return 0;
release:
	m = head;
	for ( ; i >= 0; i--) {
		mapping_free(m, comp);
		m = (void *)m + MAPPING_ITEM_SZ;
	}

	return ret;
}

static mapping_t *get_vas_mapping(comp_t *comp, unsigned long n_pages);

static vaddr_t
mm_local_get_page(unsigned long n_pages)
{
	/* get pages in the mem_mgr component. */
	unsigned long i;
	mapping_t *new_vas, *m;
	frame_t *frame = NULL;

	new_vas = get_vas_mapping(mm_comp, n_pages);

	if (!new_vas)  return 0;
	if (alloc_map_pages(new_vas, mm_comp, n_pages)) return 0;

	memset((void *)(new_vas->vaddr), 0, PAGE_SIZE*n_pages);

	return new_vas->vaddr;
}


static void
vas_ns_init(parsec_ns_t *vas, void *tbl)
{
	int i, j;

	vas->item_sz = MAPPING_ITEM_SZ;
	vas->lookup  = mapping_lookup;
	vas->alloc   = vas_alloc;
	vas->free    = vas_free;
	vas->expand  = vas_expand;

	vas->parsec = &mm;

	vas->allocator.quiesce       = frame_quiesce;  /* same as physical frame. */
	vas->allocator.qwq_min_limit = VAS_QWQ_SIZE;
	vas->allocator.qwq_max_limit = (unsigned long)(-1); //VAS_QWQ_SIZE * 4;
	vas->allocator.n_local       = NUM_CPU;

	/* FIXME: Hard coded now */
	for (i = 0; i < NUM_CPU; i++) {
		for (j = 0; j < QUIE_QUEUE_N_SLAB; j++) {
			if (j >= 8)
				vas->allocator.qwq[i].slab_queue[j].qwq_min_limit = VAS_QWQ_SIZE_SMALL;
			else
				vas->allocator.qwq[i].slab_queue[j].qwq_min_limit = VAS_QWQ_SIZE;
		}
	}

	/* Commit this last. */
	vas->tbl     = tbl;

	return;
}

static int
comp_vas_init(comp_t *c)
{
	vaddr_t p;
	parsec_ns_t *ns = &c->mapping_ns;
	int ret = 0;

	comp_lock(c);
	if (ns->tbl) {
		/* Some other thread did this before us. */
		goto done;
	}

	p = mm_local_get_page(MM_PGD_NPAGES);
	if (!p) cos_throw(done, -ENOMEM);

	vas_ns_init(ns, (void *)p);
done:
	comp_unlock(c);

	return ret;
}

static int
comp_init(int id)
{
	comp_t *c;

	c = comp_lookup(id);
	assert(c);
	c->id = id;
	ck_spinlock_ticket_init(&(c->comp_lock));

	return 0;
}

static unsigned long free_capid = MM_CAPTBL_FREE;
static unsigned long
mm_alloc_pte_cap(void)
{
	unsigned long id, new;
	do {
		id = free_capid;
		new = id + captbl_idsize(CAP_PGTBL);
	} while (cos_cas(&free_capid, id, new) != CAS_SUCCESS) ;

	return id;
}

/* Mainly initialize the vas of the mm component */
static int mm_comp_init(void)
{
	parsec_ns_t *mm_vas;
	unsigned long accum;
	mapping_t *mapping, *temp_m;
	struct quie_mem_meta *meta;
	vaddr_t pte_kmem;
	capid_t pte_cap;
	unsigned long i, j;
	int ret, comp_id = cos_spd_id();

	memset((void *)mm_own_pgd, 0, PAGE_SIZE);
	memset((void *)mm_vas_pgd, 0, MM_PTE_SIZE);
	memset((void *)mm_vas_pte, 0, MM_PTE_SIZE*MM_NPTE_NEEDED);

	/* mm_comp is a global variable for fast access. */
	mm_comp = comp_lookup(comp_id);
	comp_lock(mm_comp);

	mm_vas = &mm_comp->mapping_ns;
	vas_ns_init(mm_vas, mm_own_pgd);
	/* vas allocation of mm itself isn't as frequent. */
	mm_vas->expand = NULL;
	mm_vas->allocator.qwq_min_limit = 1;
	mm_vas->allocator.qwq_max_limit = 8;

	for (i = 0; i < NUM_CPU; i++) {
		for (j = 0; j < QUIE_QUEUE_N_SLAB; j++) {
			mm_vas->allocator.qwq[i].slab_queue[j].qwq_min_limit = 1;
		}
	}

	/* We don't mess up with the heap. Instead, get a new region. */
	comp_vas_region_alloc(mm_comp, (vaddr_t)mm_vas_pgd, PAGE_SIZE, glb_freelist_add);

	for (i = 0; i < MM_NPTE_NEEDED; i++) {
		if (comp_vas_region_alloc(mm_comp, (vaddr_t)mm_vas_pte + i*MM_PTE_SIZE, MM_PTE_SIZE, glb_freelist_add)) {
			printc("MM error: mem_mgr pte init failed\n");
			return -1;
		}
	}

	comp_unlock(mm_comp);

	return 0;
}

static void
comp_ns_init(void)
{
	/* We don't use the normal alloc/free of the component
	 * namespace. Everything is kept simple here. */
	int i;
	comp_t *c;

	if ((sizeof(mapping_t) + sizeof(struct quie_mem_meta)) > MAPPING_ITEM_SZ) {
		printc("MM init: MAPPING TBL SIZE / ORD error!\n");
		BUG();
	}
	assert(COMP_ITEM_SZ % CACHE_LINE == 0);
	memset(&comp_ns, 0, sizeof(struct parsec_ns));
	memset((void *)comp_table, 0, COMP_ITEM_SZ*MAX_NUM_COMPS);

	comp_ns.tbl     = (void *)comp_table;
	comp_ns.item_sz = COMP_ITEM_SZ;
	comp_ns.lookup  = comp_lookup;
	comp_ns.parsec  = &mm;

	for (i = 0; i < MAX_NUM_COMPS; i++) {
		comp_init(i);
	}
	mm_comp_init();

	/* printc("Mem_mgr: VAS init done. \n"); */

	return;
}

static void
mm_init(void)
{
	parsec_init(&mm);

	frame_init();
	kmem_init();

	comp_ns_init();

	printc("Mem_mgr: initialized %lu frames (%lu MBs) and %lu kernel frames.\n",
	       n_pmem, (n_pmem * PAGE_SIZE) >> 20, n_kmem);
}

/**************************/
/*** Mapping operations ***/
/**************************/

static mapping_t *
get_vas_mapping(comp_t *comp, unsigned long n_pages)
{
	if (unlikely(comp->mapping_ns.tbl == NULL)) {
		if (comp_vas_init(comp)) return NULL;
	}
	if (unlikely(!n_pages)) return NULL;

	/* all comps use the same alloc function */
	assert(comp->mapping_ns.alloc == vas_alloc);
	/* printc("cpu %d calling valloc for comp %d\n", cos_cpuid() ,comp->id); */
	return vas_alloc(&comp->mapping_ns, PAGE_SIZE*n_pages);
}

/**********************************/
/*** Public interface functions ***/
/**********************************/

vaddr_t mman_valloc(spdid_t compid, spdid_t dest, unsigned long npages)
{
	comp_t *comp;
	mapping_t *m;
	vaddr_t page = 0;

	/* printc("in valloc comp %d to %d, n pages %d\n", compid, dest, npages); */

	parsec_read_lock(&mm);

	comp = comp_lookup(dest);
	if (!comp) goto done;

	/* Alloc vaddr */
	m = get_vas_mapping(comp, npages);
	if (m) page = m->vaddr;
done:
	parsec_read_unlock(&mm);

	return page;
}

vaddr_t mman_get_page(spdid_t compid, vaddr_t addr, int npages_flags)
{
	comp_t *comp;
	mapping_t *m;
	int npages, flags, ret = 0;

	npages = npages_flags >> 16;
	flags  = npages_flags & 0xFFFF;
	/* printc("MM get page: comp %ld (cap %d), addr %d, flags %d\n", compid, comp_pt_cap(compid), addr, flags); */
	if (!npages) return 0;

	/* Alloc pmem */
	parsec_read_lock(&mm);

	comp = comp_lookup(compid);
	if (addr) {
		/* Get a new page, and map it to caller specified
		 * vaddr -- must be valloc-ed from us. */
		m = mapping_lookup(comp, addr);
	} else {
		/* Alloc vaddr */
		m = get_vas_mapping(comp, npages);
	}

	if (!m) { printc("no vas\n"); cos_throw(done, 0); }
	if (alloc_map_pages(m, comp, npages)) cos_throw(done, 0);

	ret = m->vaddr;
done:
	parsec_read_unlock(&mm);

	return ret;
}

static int
alias_mapping(comp_t *s_comp, mapping_t *s, comp_t *d_comp, mapping_t *d)
{
	int ret = -1;
	frame_t *f = frame_lookup(s->frame_id, &frame_ns);

	if (unlikely(!f)) return -EINVAL;

	frame_lock(f);
	mapping_lock(s);
	/* re-check after taking the lock */
	if (!parsec_item_active(s)) goto unlock_s;

	/* if locking failed of dest mapping, others are aliasing to the same
	 * location. */
	if (!mapping_trylock(d)) goto unlock_s;
	if (d->frame_id) {
		/* Someone nailed the dest mapping before us. */
		goto unlock_all;
	}

	ret = call_cap_op(comp_pt_cap(s_comp->id), CAPTBL_OP_CPY,
			  s->vaddr, comp_pt_cap(d_comp->id), d->vaddr, 0);
	if (ret) goto unlock_all;

	if (!s->child) {
		s->child = d;
	} else {
		/* insert to the head of the dll */
		s->child->sibling_prev = d;
		d->sibling_next = s->child;
		cos_mem_fence();
		s->child = d;
	}

	assert(d->frame_id == 0 && d->parent == NULL);
	d->parent   = s;
	d->frame_id = s->frame_id;
	ret = 0;
unlock_all:
	mapping_unlock(d);
unlock_s:
	mapping_unlock(s);
	frame_unlock(f);

	return ret;
}

vaddr_t __mman_alias_page(spdid_t s_spd, vaddr_t s_addr, u32_t d_spd_flags, vaddr_t d_addr)
{
	comp_t *src_c, *dest_c;
	mapping_t *src_m, *dest_m;
	int ret = 0;
	spdid_t d_spd = d_spd_flags >> 16;
	int flags = d_spd_flags & 0xFFFF;

	/* printc("in alias page, from comp %d @ %x to comp %d @ %x\n", s_spd, s_addr, d_spd, d_addr); */

	parsec_read_lock(&mm);

	src_c  = comp_lookup(s_spd);
	dest_c = comp_lookup(d_spd);
	if (!src_c || !dest_c) goto done;

	src_m  = mapping_lookup(src_c, s_addr);
	dest_m = mapping_lookup(dest_c, d_addr);
	if (!src_m || !dest_m) goto done;
	if (src_m->frame_id == 0 || dest_m->frame_id != 0) goto done;

	/* let's try build the mapping. */
	if (alias_mapping(src_c, src_m, dest_c, dest_m)) goto done;

	ret = d_addr;
done:
	parsec_read_unlock(&mm);

	return ret;
}

/* TODO: avoid false sharing in kernel (edge case). */
#define LTBL_ENTS (1<<20)
#define LIDS_PERCPU (LTBL_ENTS/NUM_CPU_COS)
struct liveness_id {
	unsigned long lid;
	char __padding[CACHE_LINE*4 - sizeof(unsigned long)];
} PACKED CACHE_ALIGNED;

struct liveness_id lid[NUM_CPU] CACHE_ALIGNED;

static unsigned long
cpu_get_lid(void)
{
	unsigned long new_lid, offset;
	int cpu = cos_cpuid();

	new_lid = lid[cpu].lid++;
//	new_lid = tlb_flush.glb_tlb_flush;
	offset  = LIDS_PERCPU * cpu;

	return offset + new_lid % LIDS_PERCPU;
}

static int
mapping_free(mapping_t *m, comp_t *c)
{
	mapping_t *child;
	frame_t *f;
	int ret = -1;

	mapping_lock(m);
	/* always re-check before modification. */
	if (!parsec_item_active(m)) goto done;

	/* free children recursively */
	child = m->child;
	while (child) {
		mapping_free(child, c);
		child = child->sibling_next;
	}

	if (m->frame_id) {
		/* Remove the mapping first */
		ret = call_cap_op(comp_pt_cap(c->id), CAPTBL_OP_MEMDEACTIVATE,
				  m->vaddr, cpu_get_lid(), 0, 0);
		if (ret) printc("ERROR: unmap frame %lu @ vaddr %x failed", m->frame_id, (unsigned int)m->vaddr);
		/* Free mapping, and then release the frame if we are
		 * root. Let's keep this order just to be safe. */
		ret = vas_free(m, &c->mapping_ns);
		if (m->parent == NULL) {
			/* No parent means root mapping */
			f = frame_lookup(m->frame_id, &frame_ns);
			assert(f && f->child == m);

			/* Take the frame lock. */
			frame_lock(f);
			pmem_free(f);
			f->child = NULL;
			frame_unlock(f);
		}

	} else {
		/* no frame means free vas (but valloc-ed). */
		assert(m->child == NULL && m->parent == NULL);
		/* Only free the vas in this case. */
		ret = vas_free(m, &c->mapping_ns);
	}
done:
	mapping_unlock(m);

	return ret;
}

static int
mapping_del_all(mapping_t *m, comp_t *c)
{
	mapping_t *p;
	int ret = -EINVAL;

	/* safe to access, but not modify. */
	if (mapping_free(m, c)) goto done;

	/* Still safe to access m below as long as we are in the
	 * section, which means it's not quiesced yet. */

	p = m->parent;
	if (p) {
		/* No need to lock the parent, unless we are going to modify
		 * sibling mappings. */
		mapping_lock(p);
		if (m->sibling_next) m->sibling_next->sibling_prev = m->sibling_prev;
		if (m->sibling_prev) m->sibling_prev->sibling_next = m->sibling_next;
		/* first child case */
		if (p->child == m) p->child = m->sibling_next;
		mapping_unlock(p);
	}

	ret = 0;
done:
	return ret;
}

static int
mapping_del_children(mapping_t *m, comp_t *c)
{
	mapping_t *child;
	int ret = -EINVAL;

	mapping_lock(m);
	/* always re-check before modification. */
	if (!parsec_item_active(m)) goto done;

	/* Free all the children */
	child = m->child;
	while (child) {
		mapping_free(child, c);
		child = child->sibling_next;
	}

	ret = 0;
done:
	mapping_unlock(m);

	return ret;
}

int mman_revoke_page(spdid_t compid, vaddr_t addr, int flags)
{
	comp_t *c;
	mapping_t *m;
	int ret = 0;

	/* printc("in revoke page from comp %d @ addr %x\n", compid, addr); */

	parsec_read_lock(&mm);

	c = comp_lookup(compid);
	if (unlikely(!c)) goto done;
	m = mapping_lookup(c, addr);
	if ((!m)) goto done;

	ret = mapping_del_children(m, c);
done:
	parsec_read_unlock(&mm);

	return 0;
}

/* This releases the non-head mappings. They won't go back to any
 * queue / freelist. */
static inline int
release_multipage(mapping_t *m, comp_t *c)
{
	int npages, i;
	vaddr_t page;
	mapping_t *temp;

	npages = parsec_item_size(m) / PAGE_SIZE;
	assert(parsec_item_size(m) % PAGE_SIZE == 0);

	page = m->vaddr;
	/* release non-head mappings only. */
	for (i = 1; i < npages; i++) {
		page += PAGE_SIZE;
		temp = mapping_lookup(c, page);
		if (temp) mapping_del_all(temp, c);
	}

	return 0;
}

int mman_release_page(spdid_t compid, vaddr_t addr, int flags)
{
	comp_t *c;
	mapping_t *m;
	int ret = 0;

	/* printc("in release page from comp %d @ addr %x\n", compid, addr); */

	parsec_read_lock(&mm);

	c = comp_lookup(compid);
	if (unlikely(!c)) goto done;
	m = mapping_lookup(c, addr);
	if ((!m)) goto done;

	/* A hack for now. Should solve it better. */
	if (parsec_item_size(m) > PAGE_SIZE)
		release_multipage(m, c);

	ret = mapping_del_all(m, c);
done:
	parsec_read_unlock(&mm);

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
