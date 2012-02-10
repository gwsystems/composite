Notes on Component Implementations
==================================

- Each subdirectory's name matches the interface (in
  `src/components/interface`) that components in that subdirectory
  implement.  The component implementations are in the subdirectories
  of these.

- Components can implement other interfaces in addition to the one
  encoded in their directory.  These are specified in the `INTERFACES`
  variable in their `Makefile`.

- There are two special directories that do _not_ correspond to an
  interface:

  1. `no_interface` which contains those components that do not export
     an interface.  These components typically only require the use of
     other interfaces.

  2. `tests` which contains components that simply implement unit
     tests and micro-benchmarks for the various interfaces.  All
     subdirectories that start with `unit_` implement unit tests.  If
     you want to commit any code to _Composite_, you _must_ ensure
     that these run without error.  Those subdirectories starting with
     `micro_` implement microbenchmarks.  These should help you debug
     performance problems, and keep us honest.

  3. The corresponding runscripts for the tests should share the name
     of the actual component subdirectory.  So the unit test to test
     `cbuf`s called `unit_cbuf` should have a runscript called
     `unit_cbuf.sh`.`
