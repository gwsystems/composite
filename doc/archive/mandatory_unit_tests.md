List of mandatory tests
--------------------------------

This is a list of tests that everyone should guarantee they work before commit. Always keep this list 
updated when new tests are created or existing tests become deprecated.

**unit_booters.sh** -- Simplest test. Just test for booting Composite.

**lfpu.sh, lmicro_fpu.sh** -- tests for floating-point unit. Composite should run with highest priority.

**lposix.sh** -- test for libc functions and corresponding system call warpers. Currently broken due to lack of munmap post cbuf-malloc implementation.

**unit_torrent.sh** -- test for basic torrent interface.

**unit_cbuf.sh, micro_cbuf.sh** -- cbuf tests.

**unit_cbboot_cbuf.sh, unit_cbboot_pp.sh, unit_cbboot_cbuf_fork_lock.sh** -- cbboot tests

**lcbufMalloc.sh** -- test with malloc using cbufs. Fairly rudimentary, but has already discovered one bug.

**unit_fork.sh, unit_fork_pp.sh, unit_cbboot_cbuf_fork_lock.sh, micro_fork.sh, micro_fork_pp.sh** -- various fork tests. So far, testing mainly focuses on micro_fork and unit_fork, but none of them seem broken. unit_fork is a trivial fork operation while micro_fork tries to time how efficient forking is.
