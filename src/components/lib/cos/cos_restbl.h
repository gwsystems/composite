#include <cos_compiler.h>
#include <cos_consts.h>
#include <cos_consts.h>
#include <cos_error.h>
#include <cos_types.h>

/* `o` = # of bits, output = mask of `o` 1s. E.g. o = 3 -> ...00111 = 7 */
#define COS_ORD2MASK(o) ((1 << (o)) - 1)

/**
 * `cos_restbl_intern_bits` returns bits `(B_n, B_m]` of `addr` where
 *
 * m = `node_bit_granularity`
 * n = `node_bit_granularity + node_bits`
 *
 * Thus, the return value is < 2^{n-m}.
 *
 * We assume that `node_bit_granularity + node_bits <= sizeof(uword_t) * 8`.
 *
 * - `@addr` - The address from which we'll extract bits
 * - `@node_bits` - How many bits we want to remove
 * - `@node_bit_granularity` - at what offset into add?
 * - `@return` - resulting bits
 */
COS_FORCE_INLINE static inline uword_t
cos_restbl_intern_bits(uword_t addr, uword_t node_bits, uword_t node_bit_granularity)
{
	return (addr >> node_bit_granularity) & COS_ORD2MASK(node_bits);
}

/**
 * `cos_restbl_num_nodes` finds how many blocks/nodes represent
 * addresses up to `addr` from a lower address `addr_lower` at a
 * specific level in the trie. Put another way, how many values within
 * the bits `(B_n, B_m]` (as defined in `cos_restbl_intern_bits`) are
 * necessary to represent the address range `(addr + addr_lower,
 * addr_lower]`?
 *
 * - `@addr` - the higher address of the range
 * - `@addr_lower` - the lower address of the range
 * - `@node_bits` - how many bits are represented at a level
 * - `@node_bit_granularity` - the offset of the node's bits
 * - `@return` - # of nodes needed to represent the range
 */
COS_FORCE_INLINE static inline uword_t
cos_restbl_num_nodes(uword_t addr, uword_t addr_lower, uword_t node_bits, uword_t node_bit_granularity)
{
	uword_t node_addr       = cos_restbl_intern_bits(addr, node_bits, node_bit_granularity);
	uword_t node_addr_lower = cos_restbl_intern_bits(addr_lower, node_bits, node_bit_granularity);

	/* +1 as we always require a single node */
	return node_addr - node_addr_lower + 1;
}

/**
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
 * - `@MAX_DEPTH` - depth of the radix trie
 * - `@TOP_ORD` - # of bits in the top node
 * - `@INTERN_ORD` - # of bits in internal nodes
 * - `@LEAF_ORD` - # of bits in leaf nodes
 * - `@off_ret` - The offset at which we find the capability to the captbl
 *   node if the return value is "success".
 * - `@return` - normal return value, errors if out of bound.
 */
COS_FORCE_INLINE static inline cos_retval_t
cos_restbl_node_offset(uword_t lvl, uword_t addr, uword_t addr_lower, uword_t addr_max_num, const uword_t MAX_DEPTH,
		       const uword_t TOP_ORD, const uword_t INTERN_ORD, const uword_t LEAF_ORD, uword_t *off_ret)
{
	uword_t i, off = 0;
	/* Maximum, bit-addressable address. We assume that MAX_DEPTH >= 2 (see static assertion below). */
	const uword_t addrspc_max = COS_ORD2MASK(TOP_ORD + (INTERN_ORD * (MAX_DEPTH - 2)) + LEAF_ORD);
	/* Maximum address within the slice of the namespace defined by the arguments. */
	const uword_t max_addr = addr_lower + addr_max_num - 1;

	if (lvl > MAX_DEPTH - 1 ||
	    addr_max_num + addr_lower >= addrspc_max ||
	    (addr < addr_lower || addr >= addr_lower + addr_max_num)) return -COS_ERR_OUT_OF_BOUNDS;

	for (i = 0; i <= lvl; i++) {
		/* For previous levels, assume max allocations; for the target level, lookup the addr */
		uword_t c = (i == lvl)? addr: max_addr;

		if (i == 0) {        /* only a single top node */
			off += 1;
		} else if (i == 1) { /* second level uses the top-level's properties */
			off += cos_restbl_num_nodes(c, addr_lower, TOP_ORD, LEAF_ORD + ((MAX_DEPTH - 2) * INTERN_ORD));
		} else {	     /* the rest of the levels... */
			off += cos_restbl_num_nodes(c, addr_lower, INTERN_ORD, LEAF_ORD + ((MAX_DEPTH - i - 1) * INTERN_ORD));
		}
	}
	*off_ret = off - 1; 	/* -1 here as at `lvl`, we counted past the node */

	return COS_RET_SUCCESS;
}

/**
 * `cos_restbl_intern_offset` calculates the offset into the node at
 * level `lvl` who holds the reference to the next node for address
 * `addr`. In short: find the offset for `addr` in the node at `lvl`.
 * This is meant to be used with the `RESTBL_NODE_OFFSET_GEN` macro to
 * make the radix trie values constant.
 *
 * - `@lvl` - for which level do we want to know the offset
 * - `@addr` - address we want to know offset of
 * - `@MAX_DEPTH` - trie depth
 * - `@TOP_ORD` - order of top node
 * - `@INTERN_ORD` - order of internal nodes
 * - `@LEAF_ORD` - order of leaf nodes
 * - `@off_ret` - return value
 * - `@return` - `COS_ERR_OUT_OF_BOUNDS` or success.
 */
COS_FORCE_INLINE static inline cos_retval_t
cos_restbl_intern_offset(uword_t lvl, uword_t addr, const uword_t MAX_DEPTH, const uword_t TOP_ORD,
		     const uword_t INTERN_ORD, const uword_t LEAF_ORD, uword_t *off_ret)
{
	const uword_t addrspc_max = COS_ORD2MASK(TOP_ORD + (INTERN_ORD * (MAX_DEPTH - 2)) + LEAF_ORD);

	if (lvl > MAX_DEPTH - 1 || addr >= addrspc_max) return -COS_ERR_OUT_OF_BOUNDS;

	if (lvl == MAX_DEPTH - 1) { /* Leaf needs to consider no lower-order bits  */
		*off_ret = cos_restbl_intern_bits(addr, LEAF_ORD, 0);
	} else if (lvl == 0) { /* top node considers all lower-order bits */
		*off_ret = cos_restbl_intern_bits(addr, TOP_ORD, LEAF_ORD + ((MAX_DEPTH - 2) * INTERN_ORD));
	} else {		/* internal nodes */
		*off_ret = cos_restbl_intern_bits(addr, INTERN_ORD, LEAF_ORD + ((MAX_DEPTH - 2 - lvl) * INTERN_ORD));
	}

	return COS_RET_SUCCESS;
}

/* Generate the captbl/pgtbl node offset functions with constant "shape" variables. */
#define COS_RESTBL_NODE_OFFSET_GEN(name, max_depth, top_ord, intern_ord, leaf_ord) \
	COS_STATIC_ASSERT(max_depth >= 2, "Resource tables must have at least two levels."); \
	COS_FORCE_INLINE static inline cos_retval_t			\
	cos_##name##_node_offset(uword_t lvl, uword_t addr, uword_t addr_lower, uword_t addr_max_num, uword_t *off_ret) \
	{ return cos_restbl_node_offset(lvl, addr, addr_lower, addr_max_num, max_depth, top_ord, intern_ord, leaf_ord, off_ret); } \
	static inline cos_retval_t					\
	cos_##name##_intern_offset(uword_t lvl, uword_t addr, uword_t *ret) \
	{ return cos_restbl_intern_offset(lvl, addr, max_depth, top_ord, intern_ord, leaf_ord, ret); } \
	static inline uword_t						\
	cos_##name##_num_nodes(uword_t addr_lower, uword_t addr_max_num) \
	{								\
		uword_t off_ret;					\
		cos_restbl_node_offset(max_depth - 1, addr_lower + addr_max_num - 1, addr_lower, \
				       addr_max_num, max_depth, top_ord, intern_ord, leaf_ord, &off_ret); \
		return off_ret;						\
	}

COS_RESTBL_NODE_OFFSET_GEN(captbl, COS_CAPTBL_MAX_DEPTH, COS_CAPTBL_INTERNAL_ORD, COS_CAPTBL_INTERNAL_ORD, COS_CAPTBL_LEAF_ORD)
/* Note that all pgtbl APIs are indexed by page number (pageno == address), not virtual addresses */
COS_RESTBL_NODE_OFFSET_GEN(pgtbl,  COS_PGTBL_MAX_DEPTH,  COS_PGTBL_TOP_ORD,       COS_PGTBL_INTERNAL_ORD,  COS_PGTBL_INTERNAL_ORD)
