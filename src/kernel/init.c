#include <state.h>
#include <consts.h>
#include <chal_regs.h>
#include <cos_error.h>
#include <cos_types.h>
#include <pgtbl.h>
#include <cos_consts.h>
#include <compiler.h>
#include <chal_types.h>
#include <cos_restbl.h>
#include <cos_bitmath.h>
#include <assert.h>

#include <resources.h>
#include <captbl.h>
#include <state.h>
#include <thread.h>

#include <init.h>

/*
 * `captbl_num_nodes_initial` returns the number of required
 * capability table nodes.
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
 * required number of capabilities. Based on an assumption elaborated
 * below, this requires only three iterations, thus we've unrolled
 * those.
 *
 * - `@const_caps` - The constant # of non-captbl capabilities
 * - `@return` - the number of required captbl nodes
 */
static uword_t
captbl_num_nodes_initial(uword_t const_caps)
{
	uword_t captbl_caps_required = 0;

	/*
	 * First, how many caps do we need to support the requested
	 * caps? Note that this isn't just `const_caps` as this
	 * requires capability node allocations to hold/index the
	 * `const_caps` number of capabilities.
	 */
	captbl_caps_required = cos_captbl_num_nodes(0, const_caps);
	/* How many caps are required given we have to hold captbl caps as well? */
	captbl_caps_required = cos_captbl_num_nodes(0, const_caps + captbl_caps_required);
	/* If the expands the needed captbl nodes, we might need more. */
	captbl_caps_required = cos_captbl_num_nodes(0, const_caps + captbl_caps_required);

	/*
	 * We assume that the number of required capabilities has
	 * converged at this point (we're reached a fixed-point). Each
	 * call, we should require at most
	 * `prev`/`COS_CAPTBL_LEAF_NENT` additional capabilities (for
	 * captbl nodes) where `prev` is the previous iteration's
	 * requirement. So this assumption should hold if `const_caps
	 * < 64^3 = 2^6^8 = 2^24`. As captbls are smaller than that,
	 * this assumption must hold.
	 *
	 * This analysis ignores the capabilities/nodes needed for
	 * non-leaf captbl nodes, but doing so shouldn't change the
	 * conclusion (being very conservative, we can assume that
	 * `COS_CAPTBL_MAX_DEPTH` capabilities are required for each
	 * leaf node, which changes the math to `const_caps < (64 -
	 * COS_CAPTBL_MAX_DEPTH - 1)^3`...which is still larger than
	 * the capability namespace for any reasonable captbl depth).
	 */

	return captbl_caps_required;
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
kernel_init(uword_t post_constructor_offset, struct kernel_init_state *s)
{
	int i;
	struct captbl_leaf *captbl_null;
	uword_t constructor_offset;

	*s = (struct kernel_init_state) { 0 };
	/* For usage in later APIs */
	s->post_constructor_offset = post_constructor_offset;

	chal_state_init();

	/*
	 * This set of blocks calculates the *layout* of the resources
	 * for all threads, resource tables, the component, etc...
	 */
	constructor_offset = 1;

	/*
	 * Initialize the null captbl node:
	 */
	printk("Initializing typed memory.\n\tCapability-table NULL nodes...\n");
	page_types[0] = (struct page_type) {
		.type     = COS_PAGE_TYPE_KERNEL,
		.kerntype = COS_PAGE_KERNTYPE_CAPTBL_LEAF,
		.refcnt   = 1,
		.coreid   = 0,
		.liveness = 0,
	};
	captbl_null = (struct captbl_leaf *)&pages[0];
	for (i = 0; i < COS_CAPTBL_LEAF_NENT; i++) {
		captbl_null->capabilities[i] = (struct capability_generic) {
			.type = COS_CAP_TYPE_NIL, /* Can never be used for anything */
			.liveness = 0,
			.intern = 0,
			.operations = COS_OP_NIL,
		};
	}

	/*
	 * Initialize the page-types for the pages devoted to the
	 * constructor's image. Note that the constructor's elf object
	 * should already be loaded into these pages by the linker
	 * script (`linker.ld`). We don't want to use a `resource_*`
	 * API, as it initializes the associated page.
	 */
	printk("\tConstructor virtual memory...\n");
	for (i = constructor_offset; (uword_t)i < post_constructor_offset; i++) {
		page_types[i] = (struct page_type) {
			.type     = COS_PAGE_TYPE_VM,
			.kerntype = 0,
			.refcnt   = 1,
			.coreid   = 0,
			.liveness = 0,
		};
	}

	/*
	 * Initialize the pages and page-types structures for the rest
	 * of the pages. Assume that the `pgtbl_offset` is post the
	 * captbl nil page and the component's elf image.
	 */
	printk("\tInitializing remaining memory to untyped...\n");
	for (i = post_constructor_offset; i < COS_NUM_RETYPEABLE_PAGES; i++) {
		page_zero(&pages[i]);
		page_types[i] = (struct page_type) {
			.type     = COS_PAGE_TYPE_UNTYPED,
			.kerntype = 0,
			.liveness = 0,
			.epoch    = 0,
			.refcnt   = 0,
		};
	}

	return COS_RET_SUCCESS;
}

/**
 * `kernel_cores_init()` initializes per-core, global kernel
 * data-structures. These center around the `struct state_percore` in
 * `core_state`.
 */
void
kernel_cores_init(struct kernel_init_state *state)
{
	coreid_t core;
	struct state_percore *s;

	for (core = 0; core < COS_NUM_CPU; core++) {
		/*
		 * Find the thread, assuming the threads exist one per core,
		 * starting at the thread offset.
		 */
		struct thread *t = (struct thread *)&pages[state->thread_offset + core];

		s = chal_percore_state_coreid(core);
		s->globals = (struct state) {
			.active_thread = t,
			.active_captbl = t->invstk.entries[0].component.captbl,
			.sched_thread  = t,
			.invstk_head   = 0,
		};
	}
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
 *
 * - `@post_constructor_offset` - page offset following constructor elf object
 * - `@constructor_lower_vaddr` - lowest virtual address of constructor
 * - `@constructor_entry` - entry instruction pointer virtual address
 * - `@ro_off` - offset into the object of the read-only section
 * - `@ro_sz` - size of that read-only section
 * - `@data_off` - offset into the object of the read-write section
 * - `@data_sz` - size of that read-write section
 * - `@zero_sz` - size of the bss (zero-initialized) data section
 * - `@return` - normal return value, error implies significant failure
 */
cos_retval_t
constructor_init(vaddr_t constructor_lower_vaddr, vaddr_t constructor_entry,
		 uword_t ro_off, uword_t ro_sz, uword_t data_off, uword_t data_sz, uword_t zero_sz, struct kernel_init_state *s)

{
	uword_t i, j, lvl;
	uword_t constructor_offset, post_constructor_offset, constructor_size, thread_offset, component_offset, captbl_offset, captbl_lower;
	uword_t pgtbl_offset, pgtbl_leaf_off, res_pgtbl_offset, zeroed_page_offset, frontier;
	uword_t captbl_num, pgtbl_num, caps_needed, mappings_num, res_pgtbl_num;
	uword_t constructor_lower_page, constructor_upper_page;

	post_constructor_offset = s->post_constructor_offset;

	/*
	 * This set of blocks calculates the *layout* of the resources
	 * for all threads, resource tables, the component, etc...
	 */
	constructor_offset = 1;
	constructor_size = cos_round_up_to_page(ro_sz) + cos_round_up_to_page(data_sz) + cos_round_up_to_page(zero_sz);
	zeroed_page_offset = post_constructor_offset;
	constructor_lower_page = constructor_lower_vaddr / COS_PAGE_SIZE;

	res_pgtbl_offset   = zeroed_page_offset + cos_round_up_to_pow2(zero_sz, COS_PAGE_SIZE) / COS_PAGE_SIZE;
	/* The number of pgtbl leaf nodes necessary to track all memory */
	res_pgtbl_num      = cos_round_up_to_pow2(COS_NUM_RETYPEABLE_PAGES, COS_PGTBL_LEAF_NENT) / COS_PGTBL_LEAF_NENT;

	pgtbl_offset       = res_pgtbl_offset + res_pgtbl_num;
	/* Have to map all untyped pages and *also* vaddrs */
	mappings_num       = constructor_size / COS_PAGE_SIZE;
	pgtbl_num          = cos_pgtbl_num_nodes(constructor_lower_vaddr / COS_PAGE_SIZE, mappings_num);
	constructor_upper_page = (constructor_lower_vaddr / COS_PAGE_SIZE) + mappings_num;

	/*
	 * How many captbl nodes do we need? This includes:
	 *
	 * - the empty slot for the return capability,
	 * - the hardware capability,
	 * - self component capability,
	 * - threads (though not populated),
	 * - pgtbl nodes, and
	 * - empty spaces for captbl nodes to expand for future allocations
	 */
	caps_needed        = 1 + 1 + 1 + COS_NUM_CPU + res_pgtbl_num + pgtbl_num;
	captbl_num         = captbl_num_nodes_initial(caps_needed);
	captbl_offset      = pgtbl_offset + pgtbl_num;
	component_offset   = captbl_offset + captbl_num;
	thread_offset      = component_offset + 1;
	frontier           = thread_offset + COS_NUM_CPU;

	/* To be used in future APIs */
	s->thread_offset   = thread_offset;

	printk("Constructor capability layout:\n");
	printk("\t- [0, 1) - 1 (inaccessible) captbl nil node.\n");
	printk("\t- [1, %d) - %d constructor image pages.\n", post_constructor_offset, post_constructor_offset - 1);
	printk("\t- [%d, %d) - %d constructor BSS (zeroed data) pages.\n", post_constructor_offset, res_pgtbl_offset, cos_round_up_to_pow2(zero_sz, COS_PAGE_SIZE) / COS_PAGE_SIZE);
	printk("\t- [%d, %d) - %d page-table nodes for retypeable memory.\n", res_pgtbl_offset, pgtbl_offset, res_pgtbl_num);
	printk("\t- [%d, %d) - %d page-table nodes for constructor.\n", pgtbl_offset, captbl_offset, pgtbl_num);
	printk("\t- [%d, %d) - %d cap-table nodes for constructor.\n", captbl_offset, component_offset, captbl_num);
	printk("\t- [%d, %d) - 1 component page for constructor.\n", component_offset, thread_offset);
	printk("\t- [%d, %d) - %d threads in constructor.\n", thread_offset, frontier, COS_NUM_CPU);
	printk("\t- [%d, %d) - %d remaining untyped pages.\n", frontier, COS_NUM_RETYPEABLE_PAGES, COS_NUM_RETYPEABLE_PAGES - frontier);

	/*
	 * Create the leaf-level page-table nodes, and store
	 * references to all memory pages including those that we're
	 * typing, and those that remain untyped. Note that pgtbl
	 * references to resources are not typed if they are not
	 * user-accessible, so we can retype them later in this
	 * function without complication.
	 */
	printk("Initializing kernel structures for constructor component.\n\tUntyped memory page-table allocation...\n");
	for (i = res_pgtbl_offset; i < res_pgtbl_offset + res_pgtbl_num; i++) {
		COS_CHECK(resource_restbl_create(COS_PAGE_KERNTYPE_PGTBL_LEAF, i));
	}

	printk("\tMapping of untyped memory...\n");
	for (i = 1; i < COS_NUM_RETYPEABLE_PAGES; i++) {
		COS_CHECK(pgtbl_map(res_pgtbl_offset + (i / COS_PGTBL_LEAF_NENT), i % COS_PGTBL_LEAF_NENT, i, 0));
	}

	/*
	 * Page tables for the constructor component to hold all of
	 * the initial pages and the pages for all of the resources.
	 */
	printk("\tPage-table creation...\n");
	for (lvl = 0; lvl < COS_PGTBL_MAX_DEPTH; lvl++) {
		uword_t min, max;

		/* Find the lower and upper (min/max) offsets into the page-table nodes */
		COS_CHECK(cos_pgtbl_node_offset(lvl, constructor_lower_page, constructor_lower_page, mappings_num, &min));
		COS_CHECK(cos_pgtbl_node_offset(lvl, constructor_upper_page, constructor_lower_page, mappings_num, &max));
		for (i = min; i <= max; i++) {
			COS_CHECK(resource_restbl_create(COS_PAGE_KERNTYPE_PGTBL_0 + lvl, i + pgtbl_offset));
		}
	}

	/*
	 * Capability tables to hold all page-tables, and the rest of
	 * the resources.
	 */
	printk("\tCap-table creation...\n");
	for (lvl = 0; lvl < COS_CAPTBL_MAX_DEPTH; lvl++) {
		uword_t min, max;

		COS_CHECK(cos_captbl_node_offset(lvl, 0, 0, caps_needed, &min));
		COS_CHECK(cos_captbl_node_offset(lvl, caps_needed - 1, 0, caps_needed, &max));
		for (i = min; i <= max; i++) {
			COS_CHECK(resource_restbl_create(COS_PAGE_KERNTYPE_CAPTBL_0 + lvl, i + captbl_offset));
		}
	}

	/*
	 * Component wrapping together the page and capability tables.
	 * Assumes that the first entry of the pgtbl and captbl arrays
	 * is the top.
	 */
	printk("\tComponent resource creation...\n");
	COS_CHECK(resource_comp_create(captbl_offset, pgtbl_offset, 0, constructor_entry, component_offset));

	/*
	 * Initialize the initial threads, one per core. Had to wait
	 * for the component to be created first, which required the
	 * resource tables.
	 */
	printk("\tThread creation...\n");
	for (i = 0; i < COS_NUM_CPU; i++) {
		pageref_t thread_ref = i + thread_offset;

		COS_CHECK(resource_thd_create(thread_ref, component_offset, i + 1, i, 0, thread_ref));
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
	printk("\tPage-table construction...\n");
	for (lvl = 0; lvl < COS_PGTBL_MAX_DEPTH - 1; lvl++) {
		uword_t top_off;         /* top nodes to iterate through */
		uword_t bottom_upper, bottom_lower; /* bottom node, upper and lower addresses */
		uword_t bottom_off, nentries, cons_off;

		/* Where are the top nodes? */
		COS_CHECK(cos_pgtbl_node_offset(lvl, constructor_lower_page, constructor_lower_page, mappings_num, &top_off));
		COS_CHECK(cos_pgtbl_intern_offset(lvl, constructor_lower_page, &cons_off));

		/* ...and the next level nodes to cons into the top? */
		COS_CHECK(cos_pgtbl_node_offset(lvl + 1, constructor_lower_page, constructor_lower_page, mappings_num, &bottom_lower));
		COS_CHECK(cos_pgtbl_node_offset(lvl + 1, constructor_upper_page, constructor_lower_page, mappings_num, &bottom_upper));

		nentries = (lvl == 0)? COS_PGTBL_TOP_NENT: COS_PGTBL_INTERNAL_NENT;
		/* Now cons the next level nodes into their previous level */
		for (bottom_off = bottom_lower; bottom_off <= bottom_upper; bottom_off++, cons_off = (cons_off + 1) % nentries) {
			COS_CHECK(pgtbl_construct(pgtbl_offset + top_off, cons_off, pgtbl_offset + bottom_off, 0));

			/*
			 * Have we done all of the conses for this
			 * top-level node? If so, roll over onto the
			 * next top-level node
			 */
			if (cons_off == nentries - 1) top_off++;
		}
	}

	/* ...Second, lets add the constructor's virtual memory into the page-tables... */
	printk("\tVirtual memory mapping...read-only...");
	COS_CHECK(cos_pgtbl_node_offset(COS_PGTBL_MAX_DEPTH - 1, constructor_lower_page, constructor_lower_page, mappings_num, &pgtbl_leaf_off));
	for (i = 0; i < ro_sz / COS_PAGE_SIZE; i++) {
		uword_t offset = (constructor_lower_page + i) % COS_PGTBL_LEAF_NENT;
		uword_t vm_page = constructor_offset + (ro_off / COS_PAGE_SIZE) + i;

		COS_CHECK(pgtbl_map(pgtbl_offset + pgtbl_leaf_off, offset, vm_page, COS_PGTBL_PERM_VM_EXEC));

		/* If we've reached the end of a page-table node, move on to the next */
		if (offset == (COS_PGTBL_LEAF_NENT - 1)) pgtbl_leaf_off++;
	}
	printk("RW initialized data...");
	for (j = 0; j < data_sz / COS_PAGE_SIZE; i++, j++) {
		uword_t offset = (constructor_lower_page + i) % COS_PGTBL_LEAF_NENT;
		uword_t vm_page = constructor_offset + (data_off / COS_PAGE_SIZE) + j;

		COS_CHECK(pgtbl_map(pgtbl_offset + pgtbl_leaf_off, offset, vm_page, COS_PGTBL_PERM_VM_RW));

		/* If we've reached the end of a page-table node, move on to the next */
		if (offset == (COS_PGTBL_LEAF_NENT - 1)) pgtbl_leaf_off++;
	}
	printk("RW zeroed data...\n");
	for (j = 0; j < zero_sz / COS_PAGE_SIZE; i++, j++) {
		uword_t offset = (constructor_lower_page + i) % COS_PGTBL_LEAF_NENT;
		uword_t vm_page = zeroed_page_offset + j;

		COS_CHECK(resource_vm_create(vm_page));
		COS_CHECK(pgtbl_map(pgtbl_offset + pgtbl_leaf_off, offset, vm_page, COS_PGTBL_PERM_VM_RW));

		/* If we've reached the end of a page-table node, move on to the next */
		if (offset == (COS_PGTBL_LEAF_NENT - 1)) pgtbl_leaf_off++;
	}

	/* ...Third, construct the capability table for the constructor... */
	printk("\tCaptbl construction...\n");
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
	printk("\tCapability-table population...\n");
	COS_CHECK(cos_captbl_node_offset(COS_CAPTBL_MAX_DEPTH - 1, 1, 1, captbl_num, &captbl_lower));
	/* TODO: HW cap in slot 1 */
	for (i = 0; i < captbl_num; i++) {
		uword_t ct  = captbl_offset + captbl_lower + (i / COS_CAPTBL_LEAF_NENT);
		uword_t off = (i + 2) % COS_CAPTBL_LEAF_NENT; /* offset by two for the return and HW capability */

		COS_CHECK(cap_restbl_create(ct, off, COS_PAGE_KERNTYPE_PGTBL_LEAF, COS_OP_ALL, res_pgtbl_offset + i));
	}

	return COS_RET_SUCCESS;
}

/**
 * `constructor_core_execute()` begins execution of the constructor on
 * this specific core.
 *
 * - `@coreid` - Which core is kickstarting execution? Assume that the
 *   corresponding thread id is the core plus one.
 * - `@entry_ip`  - The virtual address at which to start execution.
 */
COS_NO_RETURN void
constructor_core_execute(coreid_t coreid, struct kernel_init_state *s)
{
	struct thread *t = (struct thread *)&pages[s->thread_offset + coreid];
	struct regs *rs = &t->regs;

	userlevel_eager_return_syscall(rs);
}
