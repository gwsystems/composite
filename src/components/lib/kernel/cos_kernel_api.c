/*
 * Copyright 2015, Qi Wang and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_kernel_api.h>
#include <cos_types.h>
#include <ps_plat.h>

#include <stdarg.h>
#include <stdio.h>

#ifdef NIL
#define printd(...) printc(__VA_ARGS__)
#else
#define printd(...)
#endif

struct pgtbl_lvl_info {
	unsigned long range;
	vaddr_t (* round_to_pg)(vaddr_t );
	vaddr_t (* round_up_to_pg)(vaddr_t );
};

vaddr_t __round_to_pgt0_page(vaddr_t vaddr) { return round_to_pgt0_page(vaddr); }
vaddr_t __round_up_to_pgt0_page(vaddr_t vaddr) { return round_up_to_pgt0_page(vaddr); }

vaddr_t __round_to_pgt1_page(vaddr_t vaddr) { return round_to_pgt1_page(vaddr); }
vaddr_t __round_up_to_pgt1_page(vaddr_t vaddr) { return round_up_to_pgt1_page(vaddr); }

vaddr_t __round_to_pgt2_page(vaddr_t vaddr) { return round_to_pgt2_page(vaddr); }
vaddr_t __round_up_to_pgt2_page(vaddr_t vaddr) { return round_up_to_pgt2_page(vaddr); }

static int __capid_captbl_check_expand(struct cos_compinfo *ci);

unsigned long
cos_pgtbl_get_range(int pgtbl_lvl)
{
	switch (pgtbl_lvl)
	{
	case 0:
		return PGD_RANGE;
	case 1:
		return PGT1_RANGE;
	case 2:
		return PGT2_RANGE;
	default:
		break;
	}

	return 0;
}

vaddr_t
cos_pgtbl_round_to_page(int pgtbl_lvl, vaddr_t vaddr)
{
	switch (pgtbl_lvl)
	{
	case 0:
		return round_to_pgt0_page(vaddr);
	case 1:
		return round_to_pgt1_page(vaddr);
	case 2:
		return round_to_pgt2_page(vaddr);
	default:
		break;
	}

	return 0;
}

vaddr_t
cos_pgtbl_round_up_to_page(int pgtbl_lvl, vaddr_t vaddr)
{
	switch (pgtbl_lvl)
	{
	case 0:
		return round_up_to_pgt0_page(vaddr);
	case 1:
		return round_up_to_pgt1_page(vaddr);
	case 2:
		return round_up_to_pgt2_page(vaddr);
	default:
		break;
	}

	return 0;
}

void
cos_meminfo_init(struct cos_meminfo *mi, vaddr_t untyped_ptr, unsigned long untyped_sz, pgtblcap_t pgtbl_cap)
{
	mi->untyped_ptr = mi->umem_ptr = mi->kmem_ptr = mi->umem_frontier = mi->kmem_frontier = untyped_ptr;
	mi->untyped_frontier = untyped_ptr + untyped_sz;
	mi->pgtbl_cap        = pgtbl_cap;
}

static inline struct cos_compinfo *
__compinfo_metacap(struct cos_compinfo *ci)
{
	return ci->memsrc;
}

static inline void
cos_vasfrontier_init(struct cos_compinfo *ci, vaddr_t heap_ptr)
{
	int pgtbl_lvl;
	ci->vas_frontier = heap_ptr;

#if defined(__x86_64__)
	/*
	 * The asumption here is that the last 4-k page lower than heap_ptr has been mapped ,
	 * because loader loads the components into memory region lower than heap_ptr, so initially,
	 * heap_ptr is set to be round_up_to_page(virtual address of component's binary frontier),
	 * thus we can calculate which pate tables have been allocated, thus set corresponding page
	 * table frontiers.
	 */
	vaddr_t last_page = round_to_page(heap_ptr - 1);

	for (pgtbl_lvl = 0; pgtbl_lvl < COS_PGTBL_DEPTH - 1; pgtbl_lvl++) {
		ci->vasrange_frontier[pgtbl_lvl] = cos_pgtbl_round_up_to_page(pgtbl_lvl, last_page);
	}

#else
	/*
	 * The first allocation should trigger PTE allocation, thus we
	 * always round down to a PGD.
	 *
	 * FIXME: first allocation should if the heap_ptr is not contiguous with the
	 *        component's allocated segments I'd think.
	 *        I believe best would be to round up the heap_ptr? So you always start at
	 *        a new PGD range, which means first allocation would trigger PTE allocation?
	 */
	for (pgtbl_lvl = 0; pgtbl_lvl < COS_PGTBL_DEPTH - 1; pgtbl_lvl++) {
		ci->vasrange_frontier[pgtbl_lvl] = cos_pgtbl_round_up_to_page(pgtbl_lvl, heap_ptr);
		assert(ci->vasrange_frontier[pgtbl_lvl] == round_to_pgd_page(ci->vasrange_frontier[pgtbl_lvl]));
	}
#endif
}

static inline void
cos_capfrontier_init(struct cos_compinfo *ci, capid_t cap_frontier)
{
	int i;
	assert(round_up_to_pow2(cap_frontier, CAPMAX_ENTRY_SZ) == cap_frontier);
	ci->cap_frontier = cap_frontier;

	/*
	 * captbls are initialized populated with a single
	 * second-level node.
	 */
	if (cap_frontier < CAPTBL_EXPAND_SZ) {
		ci->caprange_frontier = round_up_to_pow2(cap_frontier, CAPTBL_EXPAND_SZ);
	} else {
		/* caprange_frontier should be rounded up to CAPTBL_EXPAND_SZ * 2 */
		ci->caprange_frontier = round_up_to_pow2(cap_frontier + CAPTBL_EXPAND_SZ, CAPTBL_EXPAND_SZ * 2) - CAPTBL_EXPAND_SZ;
	}

	for (i = 0; i < NUM_CPU; i++) {
		ci->cap16_frontier[i] = ci->cap32_frontier[i] = ci->cap64_frontier[i] = cap_frontier;
	}
}

void
cos_comp_capfrontier_update(struct cos_compinfo *ci, capid_t cap_frontier, int try_expand)
{
	if (cap_frontier <= ci->cap_frontier) return;

	if (try_expand) {
		while (cap_frontier > ci->caprange_frontier) {
			ci->cap_frontier = ci->caprange_frontier;
			__capid_captbl_check_expand(ci);
		}
	}

	cos_capfrontier_init(ci, cap_frontier);
}
void
cos_compinfo_init(struct cos_compinfo *ci, pgtblcap_t pgtbl_cap, captblcap_t captbl_cap, compcap_t comp_cap,
                  vaddr_t heap_ptr, capid_t cap_frontier, struct cos_compinfo *ci_resources)
{
	assert(ci && ci_resources);
	assert(cap_frontier % CAPMAX_ENTRY_SZ == 0);

	ci->memsrc = ci_resources;
	assert(ci_resources->memsrc == ci_resources); /* prevent infinite data-structs */

	ci->pgtbl_cap    		= pgtbl_cap;
	ci->captbl_cap   		= captbl_cap;
	ci->comp_cap     		= comp_cap;
	ci->cap_frontier 		= 0;
	ci->comp_cap_shared 	= 0;
	ci->pgtbl_cap_shared 	= 0;
	cos_vasfrontier_init(ci, heap_ptr);
	cos_capfrontier_init(ci, cap_frontier);

	ps_lock_init(&ci->cap_lock);
	ps_lock_init(&ci->mem_lock);
	ps_lock_init(&ci->va_lock);
}

/**************** [Memory Capability Allocation Functions] ***************/

static vaddr_t
__mem_bump_alloc(struct cos_compinfo *__ci, int km, int retype)
{
	vaddr_t              ret = 0;
	struct cos_compinfo *ci;
	vaddr_t *            ptr, *frontier;

	printd("__mem_bump_alloc\n");

	assert(__ci);
	ci = __compinfo_metacap(__ci);
	assert(ci && ci == __compinfo_metacap(__ci));

	ps_lock_take(&ci->mem_lock);

	if (km) {
		ptr      = &ci->mi.kmem_ptr;
		frontier = &ci->mi.kmem_frontier;
	} else {
		ptr      = &ci->mi.umem_ptr;
		frontier = &ci->mi.umem_frontier;
	}

	ret = ps_faa(ptr, PAGE_SIZE);

	if (ret >= *frontier || *frontier - ret > RETYPE_MEM_SIZE) {
		vaddr_t ptr_tmp = *ptr, front_tmp = *frontier;

		/* TODO: expand frontier if introspection says there is more memory */
		if (ci->mi.untyped_ptr == ci->mi.untyped_frontier) goto error;
		/* this is the overall frontier, so we know we can use this value... */
		ret = ps_faa(&ci->mi.untyped_ptr, RETYPE_MEM_SIZE);
		/* failure here means that someone else already advanced the frontier/ptr */
		if (ps_cas(ptr, ptr_tmp, ret + PAGE_SIZE)) {
			ps_cas(frontier, front_tmp, ret + RETYPE_MEM_SIZE);
		}
	}

	if (retype && (ret % RETYPE_MEM_SIZE == 0)) {
		/* are we dealing with a kernel memory allocation? */
		syscall_op_t op = km ? CAPTBL_OP_MEM_RETYPE2KERN : CAPTBL_OP_MEM_RETYPE2USER;
		if (call_cap_op(ci->mi.pgtbl_cap, op, ret, 0, 0, 0)) goto error;
	}

	ps_lock_release(&ci->mem_lock);

	return ret;
error:
	ps_lock_release(&ci->mem_lock);

	return 0;
}

static vaddr_t
__kmem_bump_alloc(struct cos_compinfo *ci)
{
	printd("__kmem_bump_alloc\n");
	return __mem_bump_alloc(ci, 1, 1);
}

/* this should back-up to using untyped memory... */
static vaddr_t
__umem_bump_alloc(struct cos_compinfo *ci)
{
	printd("__umem_bump_alloc\n");
	return __mem_bump_alloc(ci, 0, 1);
}

static vaddr_t
__untyped_bump_alloc(struct cos_compinfo *ci)
{
	printd("__umem_bump_alloc\n");
	return __mem_bump_alloc(ci, 1, 0);
}

/**************** [Capability Allocation Functions] ****************/

static capid_t __capid_bump_alloc(struct cos_compinfo *ci, cap_t cap);

static int
__capid_captbl_check_expand(struct cos_compinfo *ci)
{
	/* the compinfo that tracks/allocates resources */
	struct cos_compinfo *meta = __compinfo_metacap(ci);
	/* do we manage our own resources, or does a separate meta? */
	int     self_resources = (meta == ci);
	capid_t frontier, range_frontier;

	capid_t captblcap;
	capid_t captblid_add;
	vaddr_t kmem;

	/* ensure that we have bounded structure, and bounded recursion */
	assert(__compinfo_metacap(meta) == meta);

	printd("__capid_captbl_check_expand\n");
	/*
	 * Do we need to expand the capability table?
	 *
	 * This is testing the following: If we are past the first
	 * CAPTBL_EXPAND_SZ (second level in the captbl), and we are a
	 * multiple of page allocation which is _two_ second-level
	 * captbl entries.
	 *
	 * Note also that we need space in the capability table for
	 * the capability to the next node in the page-table, and
	 * perhaps for the memory capability, thus the "off by one"
	 * logic in here.
	 *
	 * Assumptions: 1. When a captbl is allocated, the first
	 * CAPTBL_EXPAND_SZ capabilities are automatically available
	 * (creation implies a second-level captbl node).  2. when we
	 * expand the captbl, we do it in CAPTBL_EXPAND_SZ times 2
	 * increments. 3. IFF the ci is used for its own memory
	 * allocation and capability table tracking, the last
	 * cache-line in each captbl internal node is reserved for the
	 * capability for the next internal node.  This will waste the
	 * rest of the entry (internal fragmentation WRT the captbl
	 * capability).  Oh well.
	 */

	if (self_resources) {
		frontier = ps_load(&ci->caprange_frontier) - CAPMAX_ENTRY_SZ;
	} else {
		frontier = ps_load(&ci->caprange_frontier);
	}
	assert(ci->cap_frontier <= frontier);

	/* Common case: */
	if (likely(ci->cap_frontier != frontier)) return 0;

	kmem = __kmem_bump_alloc(ci);
	assert(kmem); /* FIXME: should have a failure semantics for capids */

	if (self_resources) {
		captblcap = frontier;
	} else {
		/* Recursive call: can recur maximum 2 times. */
		captblcap = __capid_bump_alloc(meta, CAP_CAPTBL);
		assert(captblcap);
	}
	captblid_add = ps_load(&ci->caprange_frontier);
	assert(captblid_add % CAPTBL_EXPAND_SZ == 0);

	printd("__capid_captbl_check_expand->pre-captblactivate (%d)\n", CAPTBL_OP_CAPTBLACTIVATE);
	/* captbl internal node allocated with the resource provider's captbls */
	if (call_cap_op(meta->captbl_cap, CAPTBL_OP_CAPTBLACTIVATE, captblcap, meta->mi.pgtbl_cap, kmem, 1)) {
		assert(0); /* race condition? */
		return -1;
	}
	printd("__capid_captbl_check_expand->post-captblactivate\n");
	/*
	 * Assumption:
	 * meta->captbl_cap refers to _our_ captbl, thus
	 * captblcap's use in the following.
	 */

	/* Construct captbl */
	if (call_cap_op(ci->captbl_cap, CAPTBL_OP_CONS, captblcap, captblid_add, 0, 0)) {
		assert(0); /* race? */
		return -1;
	}

	/* Success!  Advance the frontiers. */
	frontier       = ps_load(&ci->cap_frontier);
	range_frontier = ps_faa(&ci->caprange_frontier, CAPTBL_EXPAND_SZ * 2);
	ps_cas(&ci->cap_frontier, frontier, range_frontier);

	return 0;
}

static capid_t
__capid_bump_alloc_generic(struct cos_compinfo *ci, capid_t *capsz_frontier, cap_sz_t sz)
{
	printd("__capid_bump_alloc_generic\n");
	capid_t ret;

	ps_lock_take(&ci->cap_lock);
	/*
	 * Do we need a new cache-line in the capability table for
	 * this size of capability?
	 */
	if (*capsz_frontier % CAPMAX_ENTRY_SZ == 0) {
		if (__capid_captbl_check_expand(ci)) goto error;
		*capsz_frontier = ps_faa(&ci->cap_frontier, CAPMAX_ENTRY_SZ);
	}

	ret = ps_faa(capsz_frontier, sz);
	ps_lock_release(&ci->cap_lock);

	return ret;
error:
	ps_lock_release(&ci->cap_lock);

	return 0;
}

capid_t
cos_capid_bump_alloc(struct cos_compinfo *ci, cap_t cap)
{ return __capid_bump_alloc(ci, cap); }

/* allocate a new capid in the booter. */
static capid_t
__capid_bump_alloc(struct cos_compinfo *ci, cap_t cap)
{
	unsigned long sz = captbl_idsize(cap);
	capid_t *     frontier;

	printd("__capid_bump_alloc\n");

	switch (sz) {
	case CAP16B_IDSZ:
		frontier = &ci->cap16_frontier[cos_cpuid()];
		break;
	case CAP32B_IDSZ:
		frontier = &ci->cap32_frontier[cos_cpuid()];
		break;
	case CAP64B_IDSZ:
		frontier = &ci->cap64_frontier[cos_cpuid()];
		break;
	default:
		return -1;
	}
	return __capid_bump_alloc_generic(ci, frontier, sz);
}

/**************** [User Virtual Memory Allocation Functions] ****************/

static pgtblcap_t
__bump_mem_expand_intern(struct cos_compinfo *meta, pgtblcap_t cipgtbl, vaddr_t mem_ptr, pgtblcap_t intern, int lvl)
{
	capid_t              pte_cap;
	vaddr_t              ptemem_cap;
	int                  ret;

	assert(meta == __compinfo_metacap(meta)); /* prevent unbounded structures */

	if (!intern) {
		/* if no pte cap, we activate here, and then we hook/cos this new allocated page table into its parent */
		pte_cap    = __capid_bump_alloc(meta, CAP_PGTBL);
		ptemem_cap = __kmem_bump_alloc(meta);
		/* TODO: handle the case of running out of memory */
		if (pte_cap == 0 || ptemem_cap == 0) {
			assert(0);
			return 0;
		}

		/* Current pgtbl is lvl-th, we would like to activate its next lvl page, so we use lvl + 1 */
		if (call_cap_op(meta->captbl_cap, CAPTBL_OP_PGTBLACTIVATE, pte_cap, meta->mi.pgtbl_cap, ptemem_cap,
		                lvl + 1)) {
				assert(0); /* race? */
				return 0;
		}
	} else {
		pte_cap = intern;
	}

	/*
	 * Construct the second level of the pgtbl...ignore errors due
	 * to races as they constitute "helping".
	 *
	 * FIXME:
	 * 1. if we *do* actually return an error, this seems this
	 *    seems to cause errors with allocations that start PGD
	 *    aligned in that they end up doing a double alloc for elf
	 *    creation.
	 * 2. We should clean up by deactivating the pgtbl we just
	 *    activated...or at least cache it for future use.
	 */
	call_cap_op(cipgtbl, CAPTBL_OP_CONS, pte_cap, mem_ptr, 0, 0);

	return pte_cap;
}

static vaddr_t
__bump_mem_expand_range(struct cos_compinfo *meta, pgtblcap_t cipgtbl, vaddr_t mem_ptr, unsigned long mem_sz, int pgtbl_lvl)
{
	vaddr_t addr, range;
	assert(pgtbl_lvl >=0 && pgtbl_lvl < COS_PGTBL_DEPTH - 1);


#if defined(__x86_64__)
	vaddr_t tmp_frontier;
	range		 = cos_pgtbl_get_range(pgtbl_lvl);
	tmp_frontier = cos_pgtbl_round_up_to_page(pgtbl_lvl, mem_ptr + mem_sz);

	for (addr = mem_ptr; addr < tmp_frontier; addr += range) {
		if (__bump_mem_expand_intern(meta, cipgtbl, addr, 0, pgtbl_lvl) == 0) return 0;
	}

#else
	for (addr = mem_ptr; addr < mem_ptr + mem_sz; addr += PGD_RANGE) {
		/* ignore errors likely due to races here as we want to keep expanding regardless */
		if (__bump_mem_expand_intern(meta, cipgtbl, addr, 0, 0) == 0) return 0;
	}
	assert(round_up_to_pgd_page(addr) == round_up_to_pgd_page(mem_ptr + mem_sz));
#endif

	return mem_ptr;
}

vaddr_t
cos_pgtbl_intern_alloc(struct cos_compinfo *ci, pgtblcap_t cipgtbl, vaddr_t mem_ptr, unsigned long mem_sz)
{
	return __bump_mem_expand_range(__compinfo_metacap(ci), cipgtbl, mem_ptr, mem_sz, 0);
}

pgtblcap_t
cos_pgtbl_intern_expand(struct cos_compinfo *ci, vaddr_t mem_ptr, int lvl)
{
	pgtblcap_t cap;

#if defined(__x86_64__)
	assert(lvl > 0);
#endif

	ps_lock_take(&ci->va_lock);
	if (ci->vasrange_frontier[lvl] != round_to_pgd_page(mem_ptr)) goto error;

	cap = __bump_mem_expand_intern(__compinfo_metacap(ci), ci->pgtbl_cap, mem_ptr, 0, 0);
	if (!cap) goto error;

	while (1) {
		vaddr_t tmp = ps_load(&ci->vasrange_frontier[lvl]);

		if (tmp >= mem_ptr + PGD_RANGE) break;
		/* If someone else beats us to this, then the range has been extended anyway */
		ps_cas(&ci->vasrange_frontier[lvl], tmp, tmp + PGD_RANGE);
	}

	ps_lock_release(&ci->va_lock);

	return cap;
error:
	ps_lock_release(&ci->va_lock);
	return 0;
}

int
cos_pgtbl_intern_expandwith(struct cos_compinfo *ci, pgtblcap_t intern, vaddr_t mem)
{
	ps_lock_take(&ci->va_lock);
	if (ci->vasrange_frontier[0] != round_to_pgd_page(mem)) goto error;

	if ((unsigned long)ps_faa(&ci->vasrange_frontier[0], PGD_RANGE) > round_to_pgd_page(mem)) goto error;
	if ((unsigned long)ps_faa(&ci->vas_frontier, PGD_RANGE) > round_to_pgd_page(mem)) goto error;

	if (__bump_mem_expand_intern(__compinfo_metacap(ci), ci->pgtbl_cap, mem, intern, 0) != intern) {
		ps_lock_release(&ci->va_lock);
		return 1;
	}

	ps_lock_release(&ci->va_lock);
	return 0;

error:
	ps_lock_release(&ci->va_lock);
	return -1;
}

static void
__cos_meminfo_populate(struct cos_compinfo *ci, vaddr_t untyped_ptr, unsigned long untyped_sz)
{
	vaddr_t		addr, start_addr, retaddr;
	size_t		pgtbl_lvl;
	struct cos_compinfo *meta = __compinfo_metacap(ci);

	assert(untyped_ptr == round_up_to_pgd_page(untyped_ptr));

#if defined(__x86_64__)
	for(pgtbl_lvl = 0; pgtbl_lvl < COS_PGTBL_DEPTH - 1; pgtbl_lvl++) {
		retaddr = __bump_mem_expand_range(__compinfo_metacap(ci), ci->mi.pgtbl_cap, untyped_ptr, untyped_sz, pgtbl_lvl);
		assert(retaddr);
	}
#else

	assert(untyped_sz == round_up_to_pgd_page(untyped_sz));
	retaddr = __bump_mem_expand_range(ci, ci->mi.pgtbl_cap, untyped_ptr, untyped_sz, 0);
#endif
	assert(retaddr == untyped_ptr);

	ps_lock_take(&ci->mem_lock);
	/* untyped mem from current bump pointer */
	start_addr = ps_faa(&(meta->mi.untyped_ptr), untyped_sz);
	ps_faa(&(meta->mi.untyped_frontier), untyped_sz);
	ps_lock_release(&ci->mem_lock);

	for (addr = untyped_ptr; addr < untyped_ptr + untyped_sz; addr += PAGE_SIZE, start_addr += PAGE_SIZE) {
		if (call_cap_op(meta->mi.pgtbl_cap, CAPTBL_OP_MEMMOVE, start_addr, ci->mi.pgtbl_cap, addr, 0)) BUG();
	}
}

void
cos_meminfo_alloc(struct cos_compinfo *ci, vaddr_t untyped_ptr, unsigned long untyped_sz)
{
	__cos_meminfo_populate(ci, untyped_ptr, untyped_sz);

	ci->mi.untyped_ptr = ci->mi.umem_ptr = ci->mi.kmem_ptr = ci->mi.umem_frontier = ci->mi.kmem_frontier =
	  untyped_ptr;
	ci->mi.untyped_frontier = untyped_ptr + untyped_sz;
}

static vaddr_t
__page_bump_mem_alloc(struct cos_compinfo *ci, vaddr_t *mem_addr, vaddr_t *mem_frontier, size_t sz)
{
	vaddr_t              heap_vaddr, retaddr;
	struct cos_compinfo *meta = __compinfo_metacap(ci);
	size_t               rounded, pgtbl_lvl;

	printd("__page_bump_mem_alloc\n");

	assert(sz % PAGE_SIZE == 0);
	assert(meta == __compinfo_metacap(meta)); /* prevent unbounded structures */
	heap_vaddr = ps_faa(mem_addr, sz);        /* allocate our memory addresses */

#if defined(__x86_64__)
	/* Just need to map COS_PGTBL_DEPTH - 1 levels page tables, assuming root page table is already there */
	for (pgtbl_lvl = 0; pgtbl_lvl < COS_PGTBL_DEPTH - 1; pgtbl_lvl++) {
		if (heap_vaddr + sz > ci->vasrange_frontier[pgtbl_lvl]) {
			retaddr = __bump_mem_expand_range(meta, ci->pgtbl_cap, heap_vaddr, sz, pgtbl_lvl);
			assert(retaddr);

			while (1) {
				vaddr_t tmp = ps_load(&ci->vasrange_frontier[pgtbl_lvl]);
				vaddr_t tmp_frontier;

				tmp_frontier = cos_pgtbl_round_up_to_page(pgtbl_lvl, heap_vaddr + sz);

				/* perhaps another thread already advanced the frontier? */
				if (tmp >= heap_vaddr + sz) break;
				/* If this fails, then someone else already expanded for us...win! */
				ps_cas(&ci->vasrange_frontier[pgtbl_lvl], tmp, tmp_frontier);
			}
		}
	}

#else
	rounded    = sz + (heap_vaddr - round_to_pgd_page(heap_vaddr));

	/* Do we not need to allocate PTEs? */
	if (heap_vaddr + sz <= *mem_frontier) return heap_vaddr;
	retaddr = __bump_mem_expand_range(meta, ci->pgtbl_cap, round_up_to_pgd_page(heap_vaddr), rounded, 0);

	assert(retaddr);

	while (1) {
		vaddr_t tmp = ps_load(mem_frontier);

		/* perhaps another thread already advanced the frontier? */
		if (tmp > heap_vaddr) break;
		/* If this fails, then someone else already expanded for us...win! */
		ps_cas(mem_frontier, tmp, round_up_to_pgd_page(heap_vaddr + sz));
	}
	assert(*mem_frontier > heap_vaddr);

#endif
	return heap_vaddr;
}

static vaddr_t
__page_bump_valloc(struct cos_compinfo *ci, size_t sz, size_t align)
{
	vaddr_t ret_addr = 0;
	vaddr_t rounding; // how much we need to round up sz to handle alignment

	ps_lock_take(&ci->va_lock);
	rounding = round_up_to_pow2(ci->vas_frontier, align) - ci->vas_frontier;
	sz += rounding;
	ret_addr = __page_bump_mem_alloc(ci, &ci->vas_frontier, &ci->vasrange_frontier[0], sz);
	ret_addr += rounding;
	ps_lock_release(&ci->va_lock);
	assert(ret_addr % align == 0);

	return ret_addr;
}

static vaddr_t
__page_bump_alloc(struct cos_compinfo *ci, size_t sz, size_t align)
{
	struct cos_compinfo *meta = __compinfo_metacap(ci);
	vaddr_t              heap_vaddr, heap_cursor, heap_limit;

	/*
	 * Allocate the virtual address range to map into.  This is
	 * atomic, so we will get a contiguous range of sz.
	 */
	heap_vaddr = __page_bump_valloc(ci, sz, align);
	if (unlikely(!heap_vaddr)) return 0;
	heap_limit = heap_vaddr + sz;
	assert(heap_limit > heap_vaddr);

	/*
	 * Allocate the memory to map into that virtual address. Note
	 * that each allocation is *not* performed atomically.  We
	 * ensure that the virtual range is contiguous, but not the
	 * backing memory.  If we require contiguous physical memory,
	 * then this function must be called under mutual exclusion
	 * with all other memory operations.
	 */
	for (heap_cursor = heap_vaddr; heap_cursor < heap_limit; heap_cursor += PAGE_SIZE) {
		vaddr_t umem;

		umem = __umem_bump_alloc(ci);
		if (!umem) return 0;

		/* Actually map in the memory. */
		if (call_cap_op(meta->mi.pgtbl_cap, CAPTBL_OP_MEMACTIVATE, umem, ci->pgtbl_cap, heap_cursor, PAGE_ORDER)) {
			assert(0);
			return 0;
		}
	}

	return heap_vaddr;
}

/**************** [Liveness Allocation] ****************/

/*
 * TODO: This won't be generic until we have per-component liveness
 * namespaces.  This will _only work in the low-level booter_.
 */
CACHE_ALIGNED static u32_t livenessid_frontier = BOOT_LIVENESS_ID_BASE;

static u32_t
livenessid_bump_alloc(void)
{
	return livenessid_frontier++;
}

/**************** [Kernel Object Allocation] ****************/

static int
__alloc_mem_cap(struct cos_compinfo *ci, cap_t ct, vaddr_t *kmem, capid_t *cap)
{
	printd("__alloc_mem_cap\n");

	*kmem = __kmem_bump_alloc(ci);
	if (!*kmem) return -1;
	*cap = __capid_bump_alloc(ci, ct);
	if (!*cap) return -1;
	return 0;
}

/* 
 * We're doing the thread id allocation here. The kernel initially
 * allocates N threads, one per core, and uses the coreid + 1 as the
 * thread's id. Thus, we want to start our thread ids at NUM_CPU + 2.
 */
unsigned long __thdid_alloc = NUM_CPU + 2;

thdid_t
cos_thd_id_alloc(void)
{
  unsigned long id = ps_faa(&__thdid_alloc, 1);
  thdid_t assignment = (thdid_t)id;

  assert((unsigned long)assignment == id);

  return assignment;
}

/*
 * This is here in order to simplify the thd allocation interface. 
 * It will make more sense for this data to be managed in a higher
 * level interface by the component responsible for thread creation
 * in a more complex system, but this suffices for the ping-pong 
 * test.
 */
struct {
	pgtblcap_t toplvl;      /* for page allocation */
	pgtblcap_t secondlvl;   /* for pgtbl mapping */
	ulkcap_t   curr_pg;     /* current ulk page to alloc stacks in */
	vaddr_t    pg_frontier; /* vaddr of next page to alloc */
} __cos_ulk_info;

void
cos_ulk_info_init(struct cos_compinfo *ci)
{
	__cos_ulk_info.toplvl = cos_ulk_pgtbl_create(ci, &__cos_ulk_info.secondlvl);
	__cos_ulk_info.pg_frontier = ULK_BASE_ADDR + __thdid_alloc * sizeof(struct ulk_invstk);
	assert(__cos_ulk_info.toplvl);
}

pgtblcap_t
cos_ulk_pgtbl_create(struct cos_compinfo *ci, pgtblcap_t *secondlvl)
{
	size_t     range = sizeof(struct ulk_invstk) * MAX_NUM_THREADS;
	pgtblcap_t toplvl;
	int        pgtbl_lvl;

	printd("cos_ulk_pgtbl_create\n");
	assert(ci && secondlvl);

	/* allocate a pgtbl for the ulk memory */
	toplvl = cos_pgtbl_alloc(__compinfo_metacap(ci));

	/* allocate second level and return for mapping */
	*secondlvl = __bump_mem_expand_intern(__compinfo_metacap(ci), toplvl, ULK_BASE_ADDR, 0, 0);
	if (!*secondlvl) return 0;
	/* allocate the rest of the addresses on second level */
	if (!__bump_mem_expand_range(__compinfo_metacap(ci), toplvl, ULK_BASE_ADDR + PAGE_SIZE, range - PAGE_SIZE, 0)) return 0;

	/* allocate the virtual addresses we will need in the rest of the levels*/
	for (pgtbl_lvl = 1; pgtbl_lvl < COS_PGTBL_DEPTH - 1; pgtbl_lvl++) {
		if (!__bump_mem_expand_range(__compinfo_metacap(ci), toplvl, ULK_BASE_ADDR, range, pgtbl_lvl)) return 0;
	}

	return toplvl;
}

ulkcap_t
cos_ulk_page_alloc(struct cos_compinfo *ci, pgtblcap_t ulkpt, vaddr_t uaddr)
{
	struct cos_compinfo *ci_resources = __compinfo_metacap(ci);

	vaddr_t kmem;
	capid_t cap;
	u32_t   lid = livenessid_bump_alloc();

	printd("cos_ulk_pgtbl_create\n");
	assert(ci_resources && ulkpt);

	if (__alloc_mem_cap(ci_resources, CAP_ULK, &kmem, &cap)) return 0;
	assert(kmem && (round_to_page(kmem) == kmem));
	if (call_cap_op(ci_resources->captbl_cap, CAPTBL_OP_ULK_MEMACTIVATE, cap << 16 | lid, ci_resources->mi.pgtbl_cap << 16 | ulkpt, kmem, uaddr))
		BUG();

	return cap;
}

int
cos_ulk_map_in(pgtblcap_t ptc)
{
	printd("cos_ulk_map_in\n");
	assert(ptc && __cos_ulk_info.secondlvl);
	
	if (call_cap_op(ptc, CAPTBL_OP_CONS, __cos_ulk_info.secondlvl, ULK_BASE_ADDR, 0, 0)) {
		return -1; /* page is already mapped */
	}

	return 0;
}

static ulkcap_t
__cos_thd_ulk_page_alloc(struct cos_compinfo *ci, thdid_t tid)
{	
	if (!__cos_ulk_info.toplvl) return 0;

	if (!__cos_ulk_info.curr_pg || tid % ULK_STACKS_PER_PAGE == 0) {
		__cos_ulk_info.curr_pg = cos_ulk_page_alloc(ci, __cos_ulk_info.toplvl, __cos_ulk_info.pg_frontier);
		assert(__cos_ulk_info.curr_pg);
		__cos_ulk_info.pg_frontier += PAGE_SIZE;
	}
	
	return __cos_ulk_info.curr_pg;
}

static thdcap_t
__cos_thd_alloc(struct cos_compinfo *ci, compcap_t comp, thdclosure_index_t init_data, thdid_t tid)
{
	ulkcap_t ulkcap;
	vaddr_t  kmem;
	capid_t  cap;
	int      ret;

	printd("cos_thd_alloc\n");

	assert(ci && comp > 0);

	ulkcap = __cos_thd_ulk_page_alloc(ci, tid);

	if (__alloc_mem_cap(ci, CAP_THD, &kmem, &cap)) return 0;
	assert(!(init_data & ~((1 << 16) - 1)));
	/* TODO: Add cap size checking */
	ret = call_cap_op(ci->captbl_cap, CAPTBL_OP_THDACTIVATE, (init_data << 16) | cap,
			  __compinfo_metacap(ci)->mi.pgtbl_cap << 16 | comp, kmem, ulkcap << 16 | tid);
	if (ret) BUG();

	return cap;
}

#include <cos_thd_init.h>

thdcap_t
cos_thd_alloc_ext(struct cos_compinfo *ci, compcap_t comp, thdclosure_index_t idx)
{
	thdid_t tid = cos_thd_id_alloc();

	if (idx < 1) return 0;

	return __cos_thd_alloc(ci, comp, idx, tid);
}

thdcap_t
cos_thd_alloc(struct cos_compinfo *ci, compcap_t comp, cos_thd_fn_t fn, void *data)
{
	thdid_t  tid = cos_thd_id_alloc();
	int      idx = cos_thd_init_alloc(fn, data);
	thdcap_t ret;

	if (idx < 1) return 0;
	ret = __cos_thd_alloc(ci, comp, idx, tid);
	if (!ret) cos_thd_init_free(idx);

	return ret;
}

thdcap_t
cos_initthd_alloc(struct cos_compinfo *ci, compcap_t comp)
{
	thdid_t tid = cos_thd_id_alloc();

	return __cos_thd_alloc(ci, comp, 0, tid);
}

captblcap_t
cos_captbl_alloc(struct cos_compinfo *ci)
{
	vaddr_t kmem;
	capid_t cap;

	printd("cos_captbl_alloc\n");

	assert(ci);

	if (__alloc_mem_cap(ci, CAP_CAPTBL, &kmem, &cap)) return 0;
	if (call_cap_op(ci->captbl_cap, CAPTBL_OP_CAPTBLACTIVATE, cap, __compinfo_metacap(ci)->mi.pgtbl_cap, kmem, 0))
		BUG();

	return cap;
}

pgtblcap_t
cos_pgtbl_alloc(struct cos_compinfo *ci)
{
	vaddr_t kmem;
	capid_t cap;

	printd("cos_pgtbl_alloc\n");

	assert(ci);

	if (__alloc_mem_cap(ci, CAP_PGTBL, &kmem, &cap)) return 0;
	if (call_cap_op(ci->captbl_cap, CAPTBL_OP_PGTBLACTIVATE, cap, __compinfo_metacap(ci)->mi.pgtbl_cap, kmem, 0))
		BUG();

	return cap;
}

int
cos_comp_alloc_with(struct cos_compinfo *ci, compcap_t comp, u32_t lid, captblcap_t ctc, pgtblcap_t ptc, vaddr_t entry, prot_domain_t protdom)
{
	if (call_cap_op(ci->captbl_cap, CAPTBL_OP_COMPACTIVATE, comp, (ctc << 16) | ptc, ((u64_t)protdom << 16) | lid, entry)) return 1;

	return 0;
}

compcap_t
cos_comp_alloc(struct cos_compinfo *ci, captblcap_t ctc, pgtblcap_t ptc, vaddr_t entry, prot_domain_t protdom)
{
	capid_t cap;
	u32_t   lid = livenessid_bump_alloc();

	printd("cos_comp_alloc\n");

	assert(ci && ctc && ptc && lid);

	cap = __capid_bump_alloc(ci, CAP_COMP);
	if (!cap) return 0;
	if (cos_comp_alloc_with(ci, cap, lid, ctc, ptc, entry, protdom)) BUG();

	return cap;
}

int
cos_comp_alloc_shared(struct cos_compinfo *ci, pgtblcap_t ptc, vaddr_t entry, struct cos_compinfo *ci_resources, prot_domain_t protdom)
{
	compcap_t   compc;
	captblcap_t ctc = ci->captbl_cap;

	printd("cos_compinfo_alloc_shared\n");
	assert(ptc);
	assert(ctc);
	compc = cos_comp_alloc(ci_resources, ctc, ptc, entry, protdom);
	assert(compc);

	ci->comp_cap_shared = compc;
	ci->pgtbl_cap_shared = ptc;

	return 0;
}

static pgtblcap_t
__alloc_pgtbl_secondlvl(struct cos_compinfo *ci, vaddr_t heap_ptr)
{
	pgtblcap_t pte_cap;

	pte_cap = __bump_mem_expand_intern(__compinfo_metacap(ci), ci->pgtbl_cap, heap_ptr, 0, 0);
	assert(pte_cap);

	/* 
	 * this should only be called during component initialization
	 * so I dont think this needs to happen atomically
	 */
	ci->vasrange_frontier[0] = cos_pgtbl_round_up_to_page(0, heap_ptr + PAGE_SIZE);

	return pte_cap;
}

int
cos_compinfo_alloc(struct cos_compinfo *ci, vaddr_t heap_ptr, capid_t cap_frontier, vaddr_t entry,
                   struct cos_compinfo *ci_resources, prot_domain_t protdom)
{
	pgtblcap_t  ptc;
	captblcap_t ctc;
	compcap_t   compc;
	vaddr_t     last_page;
	int         pgtbl_lvl;

	printd("cos_compinfo_alloc\n");

	ptc = cos_pgtbl_alloc(ci_resources);
	assert(ptc);
	ctc = cos_captbl_alloc(ci_resources);
	assert(ctc);
	compc = cos_comp_alloc(ci_resources, ctc, ptc, entry, protdom);
	assert(compc);

	cos_compinfo_init(ci, ptc, ctc, compc, heap_ptr, cap_frontier, ci_resources);

	ci->mi.second_lvl_pgtbl_cap  = __bump_mem_expand_intern(__compinfo_metacap(ci), ci->pgtbl_cap, heap_ptr, 0, 0);
	ci->mi.second_lvl_pgtbl_addr = heap_ptr;

	/* 
	 * This is to make sure that "the address below vas_frontier has been allocated, 
	 * follow the assumption we put in cos_vasfrontier_init()"
	 */
	last_page = round_to_page(ci->vas_frontier - 1);

	/* If previous page is in a different second level pagetable, need to allocate */
	if (cos_pgtbl_round_up_to_page(0, last_page) != cos_pgtbl_round_up_to_page(0, heap_ptr)) {
		__bump_mem_expand_intern(__compinfo_metacap(ci), ptc, round_to_page(ci->vas_frontier - 1), 0, 0);	

	}

	/* Allocate the rest of the levels */
	for (pgtbl_lvl = 1; pgtbl_lvl < COS_PGTBL_DEPTH - 1; pgtbl_lvl++) {
		__bump_mem_expand_intern(__compinfo_metacap(ci), ptc, round_to_page(ci->vas_frontier - 1), 0, pgtbl_lvl);	
	}

	return 0;
}

sinvcap_t
cos_sinv_alloc(struct cos_compinfo *srcci, compcap_t dstcomp, vaddr_t entry, invtoken_t token)
{
	capid_t cap;

	printd("cos_sinv_alloc\n");

	assert(srcci && dstcomp);

	cap = __capid_bump_alloc(srcci, CAP_COMP);
	if (!cap) return 0;
	if (call_cap_op(srcci->captbl_cap, CAPTBL_OP_SINVACTIVATE, cap, dstcomp, entry, token)) BUG();

	return cap;
}

/*
 * Arguments:
 * thdcap:  the thread to activate on snds to the rcv endpoint.
 * tcap:    the tcap to use for that execution.
 * compcap: the component the rcv endpoint is visible in.
 * arcvcap: the rcv * endpoint that is the scheduler to be activated
 *          when the thread blocks on this endpoint.
 */
arcvcap_t
cos_arcv_alloc(struct cos_compinfo *ci, thdcap_t thdcap, tcap_t tcapcap, compcap_t compcap, arcvcap_t arcvcap)
{
	capid_t cap;
	int ret;

	assert(ci && thdcap && tcapcap && compcap);

	printd("arcv_alloc: tcap cap %d\n", (int)tcapcap);

	cap = __capid_bump_alloc(ci, CAP_ARCV);
	if (!cap) return 0;
	if ((ret = call_cap_op(ci->captbl_cap, CAPTBL_OP_ARCVACTIVATE, cap, thdcap | (tcapcap << 16), compcap, arcvcap))) BUG();

	return cap;
}

asndcap_t
cos_asnd_alloc(struct cos_compinfo *ci, arcvcap_t arcvcap, captblcap_t ctcap)
{
	capid_t cap;

	assert(ci && arcvcap && ctcap);

	cap = __capid_bump_alloc(ci, CAP_ASND);
	if (!cap) return 0;
	if (call_cap_op(ci->captbl_cap, CAPTBL_OP_ASNDACTIVATE, cap, ctcap, arcvcap, 0)) BUG();

	return cap;
}

/*
 * TODO: bitmap must be a subset of existing one.
 *       but there is no such check now, violates access control policy.
 */
hwcap_t
cos_hw_alloc(struct cos_compinfo *ci, u32_t bitmap)
{
	capid_t cap;

	assert(ci);

	cap = __capid_bump_alloc(ci, CAP_HW);
	if (!cap) return 0;
	if (call_cap_op(ci->captbl_cap, CAPTBL_OP_HW_ACTIVATE, cap, bitmap, 0, 0)) BUG();

	return cap;
}

void *
cos_page_bump_allocn(struct cos_compinfo *ci, size_t sz)
{
	assert(sz % PAGE_SIZE == 0);
	return (void *)__page_bump_alloc(ci, sz, PAGE_SIZE);
}

void *
cos_page_bump_allocn_aligned(struct cos_compinfo *ci, size_t sz, size_t align)
{
	assert(sz % PAGE_SIZE == 0);
	assert(align % PAGE_SIZE == 0);

	return (void *)__page_bump_alloc(ci, sz, align);
}

void *
cos_page_bump_alloc(struct cos_compinfo *ci)
{
	return cos_page_bump_allocn(ci, PAGE_SIZE);

}

capid_t
cos_cap_cpy(struct cos_compinfo *dstci, struct cos_compinfo *srcci, cap_t srcctype, capid_t srccap)
{
	capid_t dstcap;

	assert(srcci && dstci);

	dstcap = __capid_bump_alloc(dstci, srcctype);
	if (!dstcap) return 0;

	if (call_cap_op(srcci->captbl_cap, CAPTBL_OP_CPY, srccap, dstci->captbl_cap, dstcap, 0)) BUG();

	return dstcap;
}

int
cos_cap_cpy_at(struct cos_compinfo *dstci, capid_t dstcap, struct cos_compinfo *srcci, capid_t srccap)
{
	assert(srcci && dstci);

	if (!dstcap) return 0;

	if (call_cap_op(srcci->captbl_cap, CAPTBL_OP_CPY, srccap, dstci->captbl_cap, dstcap, 0)) BUG();

	return 0;
}

/**************** [Kernel Object Operations] ****************/

int
cos_thd_switch(thdcap_t c)
{
	return call_cap_op(c, 0, 0, 0, 0, 0);
}

int
cos_thd_wakeup(thdcap_t thd, tcap_t tc, tcap_prio_t prio, tcap_res_t res)
{
	return call_cap_op(tc, CAPTBL_OP_TCAP_WAKEUP, thd, (prio << 32) >> 32, prio >> 32, res);
}

sched_tok_t
cos_sched_sync(void)
{
	static sched_tok_t stok[NUM_CPU] CACHE_ALIGNED;

	return ps_faa((unsigned long *)&stok[cos_cpuid()], 1);
}

int
cos_switch(thdcap_t c, tcap_t tc, tcap_prio_t prio, tcap_time_t timeout, arcvcap_t rcv, sched_tok_t stok)
{
#if defined(__x86_64__)
	return call_cap_op(c, tc, stok, prio, rcv, timeout);
#else
	return call_cap_op(c, (stok >> 16), tc << 16 | rcv, (prio << 32) >> 32,
	                   (((prio << 16) >> 48) << 16) | ((stok << 16) >> 16), timeout);
#endif
}

int
cos_sched_asnd(asndcap_t snd, tcap_time_t timeout, arcvcap_t srcv, sched_tok_t stok)
{
	return call_cap_op(snd, 0, srcv, stok, timeout, 0);
}

int
cos_asnd(asndcap_t snd, int yield)
{
	return call_cap_op(snd, 0, 0, 0, 0, yield);
}

int
cos_sched_rcv(arcvcap_t rcv, rcv_flags_t flags, tcap_time_t timeout,
	      int *rcvd, thdid_t *thdid, int *blocked, cycles_t *cycles, tcap_time_t *thd_timeout)
{
	unsigned long thd_state = 0;
	unsigned long cyc       = 0;
	int           ret;

	ret = call_cap_retvals_asm(rcv, 0, flags, timeout, 0, 0, &thd_state, &cyc, thd_timeout);

	*blocked = (int)(thd_state >> (sizeof(thd_state) * 8 - 1));
	*thdid   = (thdid_t)(thd_state & ((1 << (sizeof(unsigned short int) * 8)) - 1));
	*cycles  = cyc;

	if (ret >= 0 && flags & RCV_ALL_PENDING) {
		*rcvd = (ret >> 1);
		ret &= 1;
	}

	return ret;
}

int
cos_rcv(arcvcap_t rcv, rcv_flags_t flags, int *rcvd)
{
	thdid_t     tid = 0;
	int         blocked;
	cycles_t    cyc;
	int         ret;
	tcap_time_t thd_timeout;

	ret = cos_sched_rcv(rcv, flags, 0, rcvd, &tid, &blocked, &cyc, &thd_timeout);
	assert(tid == 0);

	return ret;
}

vaddr_t
cos_mem_aliasn_aligned(struct cos_compinfo *dstci, struct cos_compinfo *srcci, vaddr_t src, size_t sz, size_t align, unsigned long perm_flags)
{
	size_t i;
	vaddr_t dst, first_dst;

	assert(srcci && dstci);
	assert(sz && (sz % PAGE_SIZE == 0));
	assert(align % PAGE_SIZE == 0);

	dst = __page_bump_valloc(dstci, sz, align);
	if (unlikely(!dst)) return 0;
	first_dst = dst;

	for (i = 0; i < sz; i += PAGE_SIZE, src += PAGE_SIZE, dst += PAGE_SIZE) {
		if (call_cap_op(srcci->pgtbl_cap, CAPTBL_OP_CPY, src, dstci->pgtbl_cap, dst, perm_flags)) return 0;
	}

	return first_dst;
}

vaddr_t
cos_mem_aliasn(struct cos_compinfo *dstci, struct cos_compinfo *srcci, vaddr_t src, size_t sz, unsigned long perm_flags)
{
	return cos_mem_aliasn_aligned(dstci, srcci, src, sz, PAGE_SIZE, perm_flags);
}

vaddr_t
cos_mem_alias(struct cos_compinfo *dstci, struct cos_compinfo *srcci, vaddr_t src, unsigned long perm_flags)
{
	return cos_mem_aliasn(dstci, srcci, src, PAGE_SIZE, perm_flags);
}

int
cos_mem_alias_at(struct cos_compinfo *dstci, vaddr_t dst, struct cos_compinfo *srcci, vaddr_t src, unsigned long perm_flags)
{
	assert(srcci && dstci);

	if (call_cap_op(srcci->pgtbl_cap, CAPTBL_OP_CPY, src, dstci->pgtbl_cap, dst, perm_flags)) BUG();

	return 0;
}

int
cos_mem_alias_atn(struct cos_compinfo *dstci, vaddr_t dst, struct cos_compinfo *srcci, vaddr_t src, size_t sz, unsigned long perm_flags)
{
	size_t i;
	size_t npages;

	assert(srcci && dstci);
	assert(sz % PAGE_SIZE == 0);

	npages = sz / PAGE_SIZE;
	for (i=0; i < npages; i++) {
		if (call_cap_op(srcci->pgtbl_cap, CAPTBL_OP_CPY, src + i * PAGE_SIZE, dstci->pgtbl_cap, dst + i * PAGE_SIZE, perm_flags)) BUG();
	}

	return 0;
}

int
cos_mem_remove(pgtblcap_t pt, vaddr_t addr)
{
	assert(0);
	return 0;
}

vaddr_t
cos_mem_move(struct cos_compinfo *dstci, struct cos_compinfo *srcci, vaddr_t src)
{
	vaddr_t dst;

	assert(srcci && dstci);

	dst = __page_bump_valloc(dstci, PAGE_SIZE, PAGE_SIZE);
	if (unlikely(!dst)) return 0;

	if (call_cap_op(srcci->pgtbl_cap, CAPTBL_OP_MEMMOVE, src, dstci->pgtbl_cap, dst, 0)) BUG();

	return dst;
}

int
cos_mem_move_at(struct cos_compinfo *dstci, vaddr_t dst, struct cos_compinfo *srcci, vaddr_t src)
{
	assert(srcci && dstci);

	/* TODO */
	if (call_cap_op(srcci->pgtbl_cap, CAPTBL_OP_MEMMOVE, src, dstci->pgtbl_cap, dst, 0)) BUG();

	return 0;
}

/* TODO: generalize to modify all state */
int
cos_thd_mod(struct cos_compinfo *ci, thdcap_t tc, void *tlsaddr)
{
	return call_cap_op(ci->captbl_cap, CAPTBL_OP_THDTLSSET, tc, (word_t)tlsaddr, 0, 0);
}

/* FIXME: problems when we got to 64 bit systems with the return value */
int
cos_introspect(struct cos_compinfo *ci, capid_t cap, unsigned long op)
{
	return call_cap_op(ci->captbl_cap, CAPTBL_OP_INTROSPECT, cap, (int)op, 0, 0);
}

/***************** [Kernel Tcap Operations] *****************/

tcap_t
cos_tcap_alloc(struct cos_compinfo *ci)
{
	vaddr_t kmem;
	capid_t cap;

	printd("cos_tcap_alloc\n");
	assert(ci);

	if (__alloc_mem_cap(ci, CAP_TCAP, &kmem, &cap)) return 0;
	/* TODO: Add cap size checking */
	if (call_cap_op(ci->captbl_cap, CAPTBL_OP_TCAP_ACTIVATE, (cap << 16) | __compinfo_metacap(ci)->mi.pgtbl_cap,
	                kmem, 0, 0))
		BUG();

	return cap;
}

int
cos_tcap_transfer(arcvcap_t dst, tcap_t src, tcap_res_t res, tcap_prio_t prio)
{
	int prio_higher = (u32_t)(prio >> 32);
	int prio_lower  = (u32_t)((prio << 32) >> 32);

	return call_cap_op(src, CAPTBL_OP_TCAP_TRANSFER, dst, res, prio_higher, prio_lower);
}

int
cos_tcap_delegate(asndcap_t dst, tcap_t src, tcap_res_t res, tcap_prio_t prio, tcap_deleg_flags_t flags)
{
	u32_t yield = ((flags & TCAP_DELEG_YIELD) != 0);
	/* top bit is if we are dispatching or not */
	int prio_higher = (u32_t)(prio >> 32) | (yield << ((sizeof(yield) * 8) - 1));
	int prio_lower  = (u32_t)((prio << 32) >> 32);

	return call_cap_op(src, CAPTBL_OP_TCAP_DELEGATE, dst, res, prio_higher, prio_lower);
}

int
cos_tcap_merge(tcap_t dst, tcap_t rm)
{
	return call_cap_op(dst, CAPTBL_OP_TCAP_MERGE, rm, 0, 0, 0);
}

int
cos_hw_attach(hwcap_t hwc, hwid_t hwid, arcvcap_t arcv)
{
	return call_cap_op(hwc, CAPTBL_OP_HW_ATTACH, hwid, arcv, 0, 0);
}

int
cos_hw_detach(hwcap_t hwc, hwid_t hwid)
{
	return call_cap_op(hwc, CAPTBL_OP_HW_DETACH, hwid, 0, 0, 0);
}

int
cos_hw_cycles_per_usec(hwcap_t hwc)
{
	static int cycs = 0;

	while (cycs <= 0) cycs = call_cap_op(hwc, CAPTBL_OP_HW_CYC_USEC, 0, 0, 0, 0);

	return cycs;
}

int
cos_hw_tlb_lockdown(hwcap_t hwc, unsigned long entryid, unsigned long vaddr, unsigned long paddr)
{
	return call_cap_op(hwc, CAPTBL_OP_HW_TLB_LOCKDOWN, entryid, vaddr, paddr, 0);
}

int
cos_hw_l1flush(hwcap_t hwc)
{
	return call_cap_op(hwc, CAPTBL_OP_HW_L1FLUSH, 0, 0, 0, 0);
}

int
cos_hw_tlbflush(hwcap_t hwc)
{
	return call_cap_op(hwc, CAPTBL_OP_HW_TLBFLUSH, 0, 0, 0, 0);
}

int
cos_hw_tlbstall(hwcap_t hwc)
{
	return call_cap_op(hwc, CAPTBL_OP_HW_TLBSTALL, 0, 0, 0, 0);
}

int
cos_hw_tlbstall_recount(hwcap_t hwc)
{
	return call_cap_op(hwc, CAPTBL_OP_HW_TLBSTALL_RECOUNT, 0, 0, 0, 0);
}

int
cos_hw_cycles_thresh(hwcap_t hwc)
{
	return call_cap_op(hwc, CAPTBL_OP_HW_CYC_THRESH, 0, 0, 0, 0);
}

void
cos_hw_shutdown(hwcap_t hwc)
{
	call_cap_op(hwc, CAPTBL_OP_HW_SHUTDOWN, 0, 0, 0, 0);
}

void *
cos_hw_map(struct cos_compinfo *ci, hwcap_t hwc, paddr_t pa, unsigned int len)
{
	size_t  sz, i;
	vaddr_t va;

	assert(ci && hwc && pa && len);

	sz = round_up_to_page(len);
	va = __page_bump_valloc(ci, sz, PAGE_SIZE);
	if (unlikely(!va)) return NULL;

	for (i = 0; i < sz; i += PAGE_SIZE) {
		if (call_cap_op(hwc, CAPTBL_OP_HW_MAP, ci->pgtbl_cap, va + i, pa + i, 0)) BUG();
	}

	return (void *)va;
}


/* ----- Shared Pgtbl ------ */
int
cos_get_second_lvl(struct cos_compinfo *ci, capid_t *pgtbl_cap, vaddr_t *pgtbl_addr)
{
	if(ci->mi.second_lvl_pgtbl_cap == 0) {
		return -1;
	}
	*pgtbl_cap = ci->mi.second_lvl_pgtbl_cap;
	*pgtbl_addr = ci->mi.second_lvl_pgtbl_addr;

	return 0;
}

u32_t
cos_cons_into_shared_pgtbl(struct cos_compinfo *ci, pgtblcap_t top_lvl)
{
	capid_t pte_cap;
	vaddr_t pgtbl_addr;
	int ret;

	if(cos_get_second_lvl(ci, &pte_cap, &pgtbl_addr) != 0) {
		return -1;
	}

	if (call_cap_op(top_lvl, CAPTBL_OP_CONS, pte_cap, pgtbl_addr, 0, 0)) {
		assert(0); /* race? */
		return -1;
	}

	return 0;

}
