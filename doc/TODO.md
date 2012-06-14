Todo list of relatively small improvements/features
===================================================

This is a list of divisible chunks of work that are _not_ part of a
research agenda, rather interesting technical challenges that will
greatly benefit _Composite_.  They are grouped into modifications to
the kernel, and to components.

Kernel
------

- **Separate capability namespaces** -- Move the capability lists out
  of a global array, segmented into extents for each component, and
  into a per-spd list of length `SPD_CAP_MAX_LEN` most likely attached
  directly to the spd structure.  This has the benefit that we are
  removing one of the core kernel data-structures (capabilities), and
  instead making it part of the spd structure.  It has the downside
  that if we want to support more than the hard-coded amount of
  capabilities in the spd-structure, we will have to add complexity
  (i.e. add a cvect of capabilities greater than `SPD_CAP_MAX_LEN`).
  Only a very restricted subset of components would need this larger
  number of capabilities.

- **Native boot capability** -- Add actual boot and initialization
  code to the system (e.g. in src/platform/x86) to avoid using
  hijacking techniques.  Also add some basic drivers (keyboard, vga
  console, pci, and networking card).

- **x86-64 port** -- Do a port to x86-64.  Required in the long run to
  do massively multi-core work.

- **Fix page fault handling** -- It appears that if a page fault
  happens due to a stack pointer access, or perhaps and ip access, it
  seems that the fault is passed to Linux, not Composite.  This is a
  problem.

- **Clean up the upcall/interrupt handling code** -- Remove the
  concept of brands, and instead only have upcalls.  Simplify all of
  the upcall code where possible, and prepare it for IPI handling.

- **Generally clean up the code** -- There's quite a bit of cruft in
  the kernel.

- **** -- 

Components
----------

- **Cbufs**: 
  1. Add support for granting cbuf allocation/deallocation privileges
     to other components.  Include reference counting on cbufs, and
     cbuf tracking in the cbuf_manager.  

- **Unit Tests.** Required for:
  1. locks
  2. events
  3. vas_mgr -- extending the size of reachable memory
  4. timing functionality (e.g. timed_block)

- **Automated unit tests.** Automated system for running all unit
  tests and validating that they all pass

- **Torrents**:
  1. Return two arguments from `read/write/split`.  The second will
     determine if there is more data to `read`, more splits to be done
     (or more `write`s???).  This will avoid the superfluous call to
     `read/write/split` to get the `-EAGAIN`.
  2. Return a cbuf from `split` so that we can information about a newly
     split connection when it was created due to an internal event
     (i.e. reception of a packet to create an accepted connection, or
     received a request for an asynchronous service).
  3. `read` and `write` should include an `offset` argument. 

Build System
------------

- **Increased compile-time checking.** -- Currently, we validate at
  compile time that all of the functions that are undefined in a
  component, are satisfied by a dependency.  Both of these changes
  would involve only adding to the logic of
  `src/components/cidl/verify_completeness.py`.  We do *not* verify

  1. that if you state that you have a dependency, that you actually have
     an undefined function satisfied by that dependency, and
  2. that two dependencies don't satisfy the same undefined function.

- **Beautified build system.** -- The build system should be cleaned up
  so that the output doesn't include all of the `rm blah.o`, `*** No
  rule to make target clean.  Stop.`, `make -C foo`, etc...

CFuse
-----

- **Runscript backend** -- Output a runscript as output to CFuse.

- **More compile-time checking** -- How can resources annotate a
  graph?  How can applicative passes be added to the compiler to
  interpret different resource specification or other annotations in
  an intelligent manner?

- **DSL for CFuse** -- To avoid the annoyingly high costs of compiling
  the EDSL every time you want to generate a runscript.
