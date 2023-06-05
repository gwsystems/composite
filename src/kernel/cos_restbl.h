#include <compiler.h>
#include <chal_consts.h>
#include <cos_consts.h>
#include <cos_error.h>
#include <cos_types.h>

/* Not a public API: find how many blocks/nodes represent addresses up to `addr` (within a single level).  */
COS_FORCE_INLINE static inline uword_t
__restbl_num_nodes(uword_t addr, uword_t addr_lower, uword_t node_bits, uword_t node_bit_granularity)
{
	uword_t node_addr       = (addr       >> node_bit_granularity) & ((1 << node_bits) - 1);
	uword_t node_addr_lower = (addr_lower >> node_bit_granularity) & ((1 << node_bits) - 1);

	return node_addr - node_addr_lower + 1;
}

COS_FORCE_INLINE static inline uword_t
__restbl_offset_node(uword_t addr, uword_t node_bits, uword_t node_bit_granularity)
{
	return (addr >> node_bit_granularity) & ((1 << node_bits) - 1);
}

/*
 * `restbl_node_offset` returns the capability offset for a
 * captbl/pgtbl node at a specific level that indexes a given `addr`
 * within a referenced (internal) capability-table. Note that concrete
 * instances of `addr` might be
 *
 * - capability numbers for capability tables, or
 * - page numbers for page-tables.
 *
 * This offset assumes that the resource table nodes are laid out in
 * capabilities starting at the top-levels, and going to the
 * lower-levels. If we know the lower and the maximum addresses, we
 * can statically lay and address out all of the nodes. An example:
 *
 * ```
 * <-------- capabilities in our captbl ----------->
 * ...|(0,0)|(1,0)|(1,1)|(2,0)|(2,3)|(2,3)|...
 *    |                             ^
 *    |                             |
 *    +------ offset ---------------+
 * ```
 *
 * Each of the resource table nodes is denoted as (A,B) where A is the
 * level, and B is the offset within that level. The top level has a
 * single node. Second level nodes are sequentially *after* that node.
 * Here we assume that the number of entries in those second-level
 * nodes fits the region between the lower address, and the the
 * highest address (lower + max_num_addr), which is why we see two
 * second-level nodes. Similarly, we see the three third-level nodes.
 * This function aims to solve for the offset (`offset` in the arrow
 * above) in the "array" of the capability nodes. In this case, we're
 * asking for an address that is indexed by the 3rd the entries in the
 * 3rd-level node.
 *
 * Note that this just gives us the capability we can use to find that
 * node, it is up to the calling code to actually allocate it,
 * construct the restbl, etc...
 *
 * Out of bounds errors are returned if the requested `lvl` is too
 * high, if the requested `addr` is outside of the range provide, or
 * if the requested range exceeds the namespace size.
 *
 * This is in a public header as almost all of this will be simplified
 * away by constant-propagation + dead-code elimination.
 *
 * - `@lvl` - The node level we're looking for
 * - `@addr` - The capability for which we want to find the node
 * - `@addr_lower` - The lowest capability id represented by all of
 *   the allocated nodes.
 * - `@addr_max_num` - The maximum number of capabilities represented
 *   by the capability nodes. Thus, capabilities [addr_lower,
 *   addr_lower + addr_max_num) are represented by the nodes.
 * - `@off_ret` - The offset at which we find the capability to the captbl
 *   node if the return value is "success".
 * - `@return` - normal return value, errors if out of bound.
 */
COS_FORCE_INLINE static inline cos_retval_t
restbl_node_offset(uword_t lvl, uword_t addr, uword_t addr_lower, uword_t addr_max_num,
		   const uword_t MAX_DEPTH, const uword_t TOP_ORD, const uword_t INTERN_ORD, const uword_t LEAF_ORD, uword_t *off_ret)
{
	uword_t i, off = 0;
	/* Maximum, bit-addressable address. We assume that MAX_DEPTH >= 2 (see static assertion below). */
	const uword_t addrspc_max = ((1 << (TOP_ORD + (INTERN_ORD * (MAX_DEPTH - 2)) + LEAF_ORD)) - 1);
	/* Maximum address within the slice of the namespace defined by the arguments. */
	const uword_t max_addr = addr_lower + addr_max_num - 1;

	if (lvl > MAX_DEPTH - 1 ||
	    addr_max_num + addr_lower >= addrspc_max ||
	    (addr < addr_lower || addr >= addr_lower + addr_max_num)) return -COS_ERR_OUT_OF_BOUNDS;

	for (i = 0; i <= lvl; i++) {
		/* For previous levels, assume max allocations; for the target level, lookup the addr */
		uword_t c = (i == lvl)? addr: max_addr;

		if (i == COS_CAPTBL_MAX_DEPTH - 1) { /* Leaf needs to consider no lower-order bits  */
			off += __restbl_num_nodes(c, addr_lower, LEAF_ORD, 0);
		} else if (i == 0) {                 /* top node considers all lower-order bits */
			off += __restbl_num_nodes(c, addr_lower, TOP_ORD, LEAF_ORD + ((MAX_DEPTH - 1) * INTERN_ORD));
		} else {
			off += __restbl_num_nodes(c, addr_lower, INTERN_ORD, LEAF_ORD + ((MAX_DEPTH - i - 1) * INTERN_ORD));
		}
	}
	*off_ret = off - 1; 	/* -1 here as at `lvl`, we counted past the node */

	return COS_RET_SUCCESS;
}

/* Generate the captbl/pgtbl node offset functions with constant "shape" variables. */
#define RESTBL_NODE_OFFSET_GEN(name, max_depth, top_ord, intern_ord, leaf_ord) \
	COS_FORCE_INLINE static inline cos_retval_t			\
	name##_node_offset(uword_t lvl, uword_t addr, uword_t addr_lower, uword_t addr_max_num, uword_t *off_ret) \
	{ return restbl_node_offset(lvl, addr, addr_lower, addr_max_num, max_depth, top_ord, intern_ord, leaf_ord, off_ret); } \
	COS_STATIC_ASSERT(max_depth >= 2, "Resource tables must have at least two levels."); \
	static inline uword_t						\
	name##_num_nodes(uword_t addr_lower, uword_t addr_max_num)	\
	{								\
		uword_t off_ret;					\
		restbl_node_offset(max_depth - 1, addr_lower + addr_max_num - 1, addr_lower, \
				   addr_max_num, max_depth, top_ord, intern_ord, leaf_ord, &off_ret); \
		return off_ret;						\
	}								\
	static inline uword_t						\
	name##_intern_offset(uword_t lvl, uword_t addr)			\
	{								\
		return __restbl_offset_node(addr, 0, 0);		\
	}

RESTBL_NODE_OFFSET_GEN(captbl, COS_CAPTBL_MAX_DEPTH, COS_CAPTBL_INTERNAL_ORD, COS_CAPTBL_INTERNAL_ORD, COS_CAPTBL_LEAF_ORD)
RESTBL_NODE_OFFSET_GEN(pgtbl,  COS_PGTBL_MAX_DEPTH,  COS_PGTBL_TOP_ORD,       COS_PGTBL_INTERNAL_ORD,  COS_PGTBL_INTERNAL_ORD)
