Todo list of items relatively small improvements/features
`````````````````````````````````````````````````````````

This is a list of divisible chunks of work that are _not_ part of a
research agenda, rather interesting technical challenges that will
greatly benefit _Composite_.  They are grouped into modifications to
the kernel, and to components.

Kernel
------

- Move the capability lists out of a global array, segmented into
  extents for each component, and into a per-spd list of length
  `SPD_CAP_MAX_LEN` most likely attached directly to the spd structure.
  This has the benefit that we are removing one of the core kernel
  data-structures (capabilities), and instead making it part of the
  spd structure.  It has the downside that if we want to support more
  than the hard-coded amount of capabilities in the spd-structure, we
  will have to add complexity (i.e. add a cvect of capabilities
  greater than `SPD_CAP_MAX_LEN`).  Only a very restricted subset of
  components would need this larger number of capabilities.

- Add actual boot and initialization code to the system (e.g. in
  src/platform/x86) to avoid using hijacking techniques.  Also add
  some basic drivers (keyboard, vga console, pci, and networking
  card).

- Do a port to x86-64.  Required in the long run to do massively
  multi-core work.

Components
----------

- Cbufs: 
  1. Port `cbuf_vect` code to `cvect` code.
  2. Add support for granting cbuf allocation/deallocation privileges
     to other components.  Include reference counting on cbufs, and
     cbuf tracking in the cbuf_manager.  
  3. Add `cbuf_vect_lookup_addr` to get the address of the vect entry.
     This will allow i) atomic updates of the structure and ii)
     greater efficiency as we won't have lookups being done 10 times
     per function.
  4. Make the cbuf slab vector contain _either_ a pointer to the slab
     (for small cbuf allocations), or the actual structure containing
     `(cbuf_meta *,freelist_next)` tuples.  This will avoid slabs, and their
     allocation in the case of larger allocations.  This will enable a
     time-space trade-off to be made by component designers.

- Unit Tests required for:
  1. torrents
  2. locks
  3. events
  4. timing functionality (e.g. timed_block)
  5. simple IPC (in the style of ping/pong)
  6. memory mapping and allocation
  7. virtual memory allocations (valloc)
  8. vas_mgr -- extending the size of reachable memory

- Automated system for running all unit tests and validating that they
  all pass

