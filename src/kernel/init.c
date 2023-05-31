#include <cos_consts.h>
#include <compiler.h>
#include <chal_types.h>

#include <resources.h>
#include <captbl.h>
#include <state.h>
#include <thread.h>

/**
 * `kernel_init()` initializes global kernel data-structures. These
 * include:
 *
 * - pages in `pages`,
 * - page type meta-data in `page_types`, and
 * - "NULL" capability-table nodes (page and type).
 */
void
kernel_init(uword_t init_page_start)
{
	int i;
	struct captbl_leaf *captbl_null;

	/* Initialize the page-types for the pages devoted to the constructor's image */
	for (i = 1; i < init_page_start; i++) {
		page_types[i] = (struct page_type) {
			.type = COS_PAGE_TYPE_VM,
			.kerntype = 0,
			.refcnt = 1,
			.coreid = 0,
			.liveness = 0,
		};
	}
	/*
	 * Assume that the `init_page_start` is post the captbl nil
	 * page, and the component's elf image.
	 */
	for (i = init_page_start; i < COS_NUM_RETYPEABLE_PAGES; i++) {
		page_zero(&pages[i]);
		page_types[i] = (struct page_type) {
			.type = COS_PAGE_TYPE_UNTYPED,
			.kerntype = 0,
		};
	}

	/* The null captbl node: */
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
 *
 * -
 * - Page type meta-data.
 */
COS_NO_RETURN void
constructor_core_execute(coreid_t coreid)
{

}
