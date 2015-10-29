# Tests

- Test remote frees
- Test SMR with varying levels of batch size
- Test that the memory utilization in SMR converges on some specific size.

# Benchmarks

- namespace lookup
- ns alloc/dealloc

# Features

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
