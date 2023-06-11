#include "chal_consts.h"
#include "cos_error.h"
#include "cos_types.h"
#include "pgtbl.h"
#include <cos_consts.h>
#include <compiler.h>
#include <chal_types.h>
#include <cos_restbl.h>

#include <resources.h>
#include <captbl.h>
#include <state.h>
#include <thread.h>

static uword_t
captbl_initial_fixedpoint(uword_t const_caps, uword_t captbl_node_num)
{
	return cos_captbl_num_nodes(0, captbl_node_num + const_caps);
}

/*
 * `captbl_initial` returns the number of required capability table
 * nodes.
 *
 * We have an annoying set of dependencies here: to know how many
 * captbl nodes we need, we have to know how many capabilities
 * are required. But to know how many capabilities are
 * required, we have to know how many are devoted to the
 * capability table. This would trivially solved if the captbl
 * could be last, but we need it *before* the component, thus
 * before the threads.
 *
 * So the solution is that we need to reach a fixed point on the
 * required number of capabilities. Given that each capability node
 * holds 64 capabilities, three iterations should reach fixed point.
 *
 * - `@const_caps` - The constant # of non-captbl capabilities
 * - `@return` - the number of required captbl nodes
 */
static uword_t
captbl_initial(uword_t const_caps)
{
	uword_t prev = 0;

	prev = captbl_initial_fixedpoint(const_caps, prev);
	prev = captbl_initial_fixedpoint(const_caps, prev);

	return captbl_initial_fixedpoint(const_caps, prev);
}

/**
 * `kernel_init()` initializes global kernel data-structures. These
 * include:
 *
 * - pages in `pages`,
 * - page type meta-data in `page_types`, and
 * - "NULL" capability-table nodes (page and type).
 * - page-tables that includes both references to all pages and to
 *   constructor virtual addresses, and
 * - capability-tables
 *
 * Assumes:
 *
 * - `constructor_lower_vaddr`, `ro_off`, `data_off`, `ro_sz`,
 *   `data_sz`, and `zero_sz` are all exactly divisible by
 *   `COS_PAGE_SIZE`.
 * - In virtual addresses: the zero (bss) section directly follows
 *   data, which directly follows read-only mappings.
 * - elf sections don't overlap: `ro_off + ro_sz <= data_off`
 * - elf sections fit within the elf object:
 *   `ro_sz + data_sz <= (post_constructor_offset - 1) * COS_PAGE_SIZE`
 * - elf entry address is in the code:
 *   `constructor_entry > constructor_lower_vaddr && constructor_entry <= (constructor_lower_vaddr + ro_sz)`
 *
 * The arguments include information about which pages/resources
 * contain the constructor's elf image, and the offsets within that
 * image of the various sections. An implicit argument is where the
 * constructor's elf object starts in the resource array, which is
 * hard-coded by the linker script to `1`.
 *
 * - `@post_constructor_offset` - page offset directly past the constructor
 * - `@constructor_lower_vaddr` - the lowest virtual addr of the constructor
 * - `@constructor_entry` - address at which to begin execution
 * - `@ro_off` - offset into the constructor of the read-only data
 * - `@ro_sz` - size of that section (multiple of `COS_PAGE_SIZE`)
 * - `@data_off` - offset into the constructor of the data
 * - `@data_sz` - size of that section (multiple of `COS_PAGE_SIZE`)
 * - `@zero_sz` - size of the "zeroed" (BSS) section (multiple of `COS_PAGE_SIZE`)
 * - `@return` - normal return error, should be `COS_RET_SUCCESS`.
 */
cos_retval_t
kernel_init(uword_t post_constructor_offset, vaddr_t constructor_lower_vaddr, vaddr_t constructor_entry,
	    uword_t ro_off, uword_t ro_sz, uword_t data_off, uword_t data_sz, uword_t zero_sz)
{
	int i, lvl;
	struct captbl_leaf *captbl_null;
	uword_t constructor_offset, constructor_size, thread_offset, component_offset, captbl_offset, captbl_lower, captbl_iter;
	uword_t pgtbl_offset, pgtbl_leaf_off, pgtbl_iter, res_pgtbl_offset, zeroed_page_offset, frontier;
	uword_t captbl_num, pgtbl_num, mappings_num, res_pgtbl_num;

	/*
	 * This set of blocks calculates the *layout* of the resources
	 * for all threads, resource tables, the component, etc...
	 */
	constructor_offset = 1;
	constructor_size = ro_sz + data_sz + zero_sz;
	zeroed_page_offset = post_constructor_offset;

	res_pgtbl_offset   = zeroed_page_offset + cos_round_up_to_pow2(zero_sz, COS_PAGE_SIZE);
	/* The number of pgtbl leaf nodes necessary to track all memory */
	res_pgtbl_num      = cos_round_up_to_pow2(COS_NUM_RETYPEABLE_PAGES, COS_PGTBL_LEAF_NENT) / COS_PGTBL_LEAF_NENT;

	pgtbl_offset       = res_pgtbl_offset + res_pgtbl_num;
	/* Have to map all untyped pages and *also* vaddrs */
	mappings_num       = cos_round_up_to_pow2(constructor_size, COS_PAGE_SIZE) / COS_PAGE_SIZE;
	pgtbl_num          = cos_pgtbl_num_nodes(constructor_lower_vaddr, mappings_num);

	/*
	 * How many captbl nodes do we need? This includes:
	 *
	 * - the empty slot for the return capability,
	 * - the hardware capability,
	 * - threads (though not populated),
	 * - pgtbl nodes, and
	 * - empty spaces for captbl nodes to expand for future allocations
	 */
	captbl_num         = captbl_initial(1 + 1 + COS_NUM_CPU + res_pgtbl_num);
	captbl_offset      = pgtbl_offset + pgtbl_num;
	component_offset   = captbl_offset + captbl_num;
	thread_offset      = component_offset + 1;
	frontier           = thread_offset + COS_NUM_CPU;

	/*
	 * Initialize the null captbl node:
	 */
	captbl_null = (struct captbl_leaf *)&pages[0];
	for (i = 0; i < COS_CAPTBL_LEAF_NENT; i++) {
		struct capability_generic *cap = &captbl_null->capabilities[i];

		*cap = (struct capability_generic) {
			.type = COS_CAP_TYPE_NIL, /* Can never be used for anything */
			.liveness = 0,
			.intern = 0,
			.operations = COS_OP_NIL,
		};
	}
	page_types[0] = (struct page_type) {
		.type = COS_PAGE_TYPE_KERNEL,
		.kerntype = COS_PAGE_KERNTYPE_CAPTBL_LEAF,
		.refcnt = 1,
		.coreid = 0,
		.liveness = 0,
	};

	/*
	 * Initialize the page-types for the pages devoted to the
	 * constructor's image. Note that the constructor's elf object
	 * should already be loaded into these pages by the linker
	 * script (`linker.ld`). We don't want to use a `resource_*`
	 * API, as it initializes the associated page.
	 */
	for (i = constructor_offset; i < res_pgtbl_offset; i++) {
		page_types[i] = (struct page_type) {
			.type = COS_PAGE_TYPE_VM,
			.kerntype = 0,
			.refcnt = 1,
			.coreid = 0,
			.liveness = 0,
		};
	}

	/*
	 * Initialize the pages and page-types structures for the rest
	 * of the pages. Assume that the `pgtbl_offset` is post the
	 * captbl nil page and the component's elf image.
	 */
	for (i = pgtbl_offset; i < COS_NUM_RETYPEABLE_PAGES; i++) {
		page_zero(&pages[i]);
		page_types[i] = (struct page_type) {
			.type = COS_PAGE_TYPE_UNTYPED,
			.kerntype = 0,
		};
	}

	/*
	 * Create the leaf-level page-table nodes, and store
	 * references to all memory pages including those that we're
	 * typing, and those that remain untyped. Note that pgtbl
	 * references to resources are not typed if they are not
	 * user-accessible, so we can retype them later in this
	 * function without complication.
	 */
	for (i = res_pgtbl_offset; i < res_pgtbl_offset + res_pgtbl_num; i++) {
		COS_CHECK(resource_restbl_create(COS_PAGE_KERNTYPE_PGTBL_LEAF, i));
	}
	for (i = 1; i < COS_NUM_RETYPEABLE_PAGES; i++) {
		COS_CHECK(pgtbl_map(res_pgtbl_offset + i / COS_PGTBL_LEAF_NENT, i % COS_PGTBL_LEAF_NENT, i, 0));
	}

	/*
	 * Page tables for the constructor component to hold all of
	 * the initial pages and the pages for all of the resources.
	 */
	pgtbl_iter = pgtbl_offset;
	for (lvl = 0; lvl < COS_PGTBL_MAX_DEPTH; lvl++) {
		uword_t max;

		/* FIXME: this logic expects the # of nodes, not the offset */
		COS_CHECK(cos_pgtbl_node_offset(lvl, mappings_num - 1, constructor_lower_vaddr, mappings_num, &max));
		for (i = pgtbl_iter; i <= max; i++) {
			COS_CHECK(resource_restbl_create(COS_PAGE_KERNTYPE_PGTBL_0 + lvl, i));
		}
		pgtbl_iter = max + 1;
	}

	/*
	 * Capability tables to hold all page-tables, and the rest of
	 * the resources.
	 */
	captbl_iter = captbl_offset;
	for (lvl = 0; lvl < COS_CAPTBL_MAX_DEPTH; lvl++) {
		uword_t max;

		COS_CHECK(cos_captbl_node_offset(lvl, frontier - 1, 0, frontier, &max));
		for (i = captbl_iter; i <= max; i++) {
			COS_CHECK(resource_restbl_create(COS_PAGE_KERNTYPE_CAPTBL_0 + lvl, i));
		}
		captbl_iter = max + 1;
	}

	/*
	 * Component wrapping together the page and capability tables.
	 * Assumes that the first entry of the pgtbl and captbl arrays
	 * is the top.
	 */
	COS_CHECK(resource_comp_create(captbl_offset, pgtbl_offset, 0, constructor_entry, component_offset));

	/*
	 * Initialize the initial threads, one per core. Had to wait
	 * for the component to be created first, which required the
	 * resource tables.
	 */
	for (i = thread_offset; i < COS_NUM_CPU; i++) {
		COS_CHECK(resource_thd_create(i, component_offset, i - thread_offset + 1, constructor_entry, 0, i));
		page_types[i].coreid = i - thread_offset;
	}

	/*
	 * All of the resources are created, and we understand their
	 * layout. Lets link together the nodes of the captbls and
	 * pgtbls.
	 *
	 * First, the constructor's page-table must have internal
	 * links to construct the tries.
	 *
	 * ...
	 */
	for (lvl = 0; lvl < COS_PGTBL_MAX_DEPTH - 1; lvl++) {
		uword_t top_off;         /* top nodes to iterate through */
		uword_t bottom_upper, bottom_lower; /* bottom node, upper and lower addresses */
		uword_t bottom_off, nentries, cons_off;

		/* Where are the top nodes? */
		COS_CHECK(cos_pgtbl_node_offset(lvl, constructor_lower_vaddr, constructor_lower_vaddr, constructor_size, &top_off));
		COS_CHECK(cos_pgtbl_intern_offset(lvl, constructor_lower_vaddr, &cons_off));

		/* ...and the next level? */
		COS_CHECK(cos_pgtbl_node_offset(lvl + 1, constructor_lower_vaddr, constructor_lower_vaddr, constructor_size, &bottom_lower));
		COS_CHECK(cos_pgtbl_node_offset(lvl + 1, constructor_lower_vaddr + constructor_size - 1, constructor_lower_vaddr, constructor_size, &bottom_upper));

		nentries = (lvl == 0)? COS_PGTBL_TOP_NENT: COS_PGTBL_INTERNAL_NENT;
		for (bottom_off = bottom_lower; bottom_off < bottom_upper; bottom_off++, cons_off = (cons_off + 1) % nentries) {
			COS_CHECK(pgtbl_construct(pgtbl_offset + top_off, cons_off, pgtbl_offset + bottom_off, 0));

			/* roll over onto the next top-level node */
			if (cons_off == nentries - 1) top_off++;
		}
	}

	/* ...Second, lets add the constructor's virtual memory into the page-tables... */
	COS_CHECK(cos_pgtbl_node_offset(COS_PGTBL_MAX_DEPTH - 1, constructor_lower_vaddr, constructor_lower_vaddr, constructor_size, &pgtbl_leaf_off));
	for (i = 0; i < constructor_size / COS_PAGE_SIZE; i++) {
		uword_t offset = ((constructor_lower_vaddr / COS_PAGE_SIZE) + i) % COS_PGTBL_LEAF_NENT;
		uword_t vm_page, perm;

		/*
		 * Find the page-we're mapping in, and its
		 * permissions. This depends on if the page is in the
		 * read-only section, the read-write (data) section,
		 * or in the zeroed-memory section (bss).
		 */
		if (i < ro_sz / COS_PAGE_SIZE) { /* read-only section */
			vm_page = constructor_offset + (ro_off / COS_PAGE_SIZE) + i;
			perm = 0;
		} else if (i < (data_sz + ro_sz) / COS_PAGE_SIZE) { /* read-write section */
			vm_page = constructor_offset + (data_off / COS_PAGE_SIZE) + i - (ro_sz / COS_PAGE_SIZE);
			perm = 0;
		} else if (i < (data_sz + ro_sz + zero_sz) / COS_PAGE_SIZE) { /* bss/zero-data section */
			vm_page = zeroed_page_offset + i - ((ro_sz + data_sz) / COS_PAGE_SIZE);
			perm = 0;
		} else {	/* this should never happen... */
			return -COS_ERR_OUT_OF_BOUNDS;
		}

		COS_CHECK(pgtbl_map(pgtbl_offset + pgtbl_leaf_off, offset, vm_page, perm));

		/* If we've reached the end of a page-table node, move on to the next */
		if (offset == (COS_PGTBL_LEAF_NENT - 1)) pgtbl_leaf_off++;
	}

	/* ...Third, construct the capability table for the constructor... */
	for (lvl = 0; lvl < COS_CAPTBL_MAX_DEPTH - 1; lvl++) {
		uword_t top_off;         /* top nodes to iterate through */
		uword_t bottom_upper, bottom_lower; /* bottom node, upper and lower addresses */
		uword_t bottom_off, nentries, cons_off;

		/* Where are the top nodes? */
		COS_CHECK(cos_captbl_node_offset(lvl, 1, 1, captbl_num, &top_off));
		COS_CHECK(cos_captbl_intern_offset(lvl, 1, &cons_off));

		COS_CHECK(cos_captbl_node_offset(lvl + 1, 1, 1, captbl_num, &bottom_lower));
		COS_CHECK(cos_captbl_node_offset(lvl + 1, 1 + captbl_num - 1, 1, captbl_num, &bottom_upper));

		nentries = COS_CAPTBL_INTERNAL_NENT;
		for (bottom_off = bottom_lower; bottom_off < bottom_upper; bottom_off++, cons_off = (cons_off + 1) % nentries) {
			COS_CHECK(captbl_construct(captbl_offset + top_off, cons_off, captbl_offset + bottom_off));

			/* roll over onto the next top-level node */
			if (cons_off == nentries - 1) top_off++;
		}
	}

	/* ...Finally, populate the capability-tables */
	COS_CHECK(cos_captbl_node_offset(COS_CAPTBL_MAX_DEPTH - 1, 1, 1, captbl_num, &captbl_lower));
	for (i = captbl_lower; i < captbl_num; i++) {
		COS_CHECK(cap_restbl_create(captbl_lower + captbl_offset));
	}

	return COS_RET_SUCCESS;
}

/**
 * `kernel_core_init()` initializes per-core, global kernel
 * data-structures. These include:
 *
 * - the `struct state_percore` in `core_state`
 */
void
kernel_core_init(uword_t start_page, coreid_t coreid)
{
	extern struct state_percore core_state[COS_NUM_CPU];
	/* The 1 here is for the null captbl page */
	struct thread *t = (struct thread *)&pages[start_page + coreid];

	core_state[coreid] = (struct state_percore) {
		.registers = { 0 },
		.active_thread = t,
		.active_captbl = t->invstk.entries[0].component.captbl,
		.sched_thread = t,
	};
}

/**
 * `constructor_init()` initializes the data-structures for the
 * booter/constructor component. These include:
 *
 * - page-table nodes,
 * - capability table nodes,
 * - component page, and
 * - initial threads.
 *
 * The main challenge here is the initialization of the capability-table
 * that includes references to the other resources *and* to itself.
 */
void
constructor_init()
{
	int captbl_root_off, captbl_leaf_off, captbl_leaf_nent;
	int pgtbl0_off, pgtbl1_off, pgtbl2_off, pgtbl3_off, pgtbl3_nent;

	captbl_num_nodes();
}

/**
 * `constructor_core_execute()` begins execution of the constructor on
 * this specific core.
 */
COS_NO_RETURN void
constructor_core_execute(coreid_t coreid)
{

}
