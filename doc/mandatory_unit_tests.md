List of mandatory tests
--------------------------------

This is a list of tests that everyone should guarantee they work before commit. Always keep this list 
updated when new tests are created or existing tests become deprecated.

**unit_booters.sh** -- Simplest test. Just test for booting Composite.

**lfpu.sh, lmicro_fpu.sh** -- tests for floating-point unit. Composite should run with highest priority.

**lposix.sh** -- test for libc functions and corresponding system call warpers.

**unit_torrent.sh** -- test for basic torrent interface.

**unit_cbuf.sh, micro_cbuf.sh** -- cbuf tests.

**unit_fork.sh, unit_fork_pp.sh, micro_fork.sh, micro_fork_pp.sh** -- fork tests

**unit_cbboot_cbuf.sh, unit_cbboot_pp.sh, unit_cbboot_cbuf_fork_lock.sh** -- cbboot tests
