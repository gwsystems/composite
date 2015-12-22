# Tests

- Test SMR with varying levels of batch size
- Test that the memory utilization in SMR converges on some specific size.

# Benchmarks

- namespace lookup
- ns alloc/dealloc

# Features

- NUMA awareness in the SMR
- Destructors for SMR memory
- Customizable quiescence functions for SMR memory.
- Lower the amount of memory saved in the SMR lists.
- Atomic operations on all the freelists
- Add a policy for which slab to remove memory from (based on utilization)
- Cache coloring in the slab, where possible
- Linked list that is SMR-interoperable (`rcu_list` equivalent)
- Page (and page extent) manager to avoid mmap/munmap calls everywhere
- When retrieving remotely freed memory, move it to a local list, and
  bound the number of items added into slabs (to bound execution time)
- Save header memory for slab (without smr) by making the smr stuff a header _before_ the slab info
  struct ps_mheader { union {struct ps_slab *slab; struct ps_mheader *n; } u; };
  struct ps_sheader { ps_free_token_t tsc; struct ps_sheader *n; struct ps_mheader m; };
- Deallocate non-leaf levels of the lookup table
