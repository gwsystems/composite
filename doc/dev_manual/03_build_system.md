# Build System and Software Construction

The Composite user-level has three main bodies of code:

- Components that implement and depend on different functional interfaces.
- Interfaces that provide a functional specification for a given set of functionality, including the serialization and deserialization logic, and potentially different implementation variants.
- Libraries which are statically linked into components.

Their structure departs from a POSIX organization, and is more similar to language-specific dependency management.
However, because this dependency management is at the level of the C ABI, it should be portable across any languages that FFI to C (i.e. most of them).

## Problems Solved

The problems solved by the library system in Composite include:

- We want to be able to share library functionality between different components.
- As such, components must somehow understand which directories to use in their include paths, library paths, and with which libraries to compile.

Composite does *not* solve the following issues:

- We do not attempt to solve library versioning through dependency management.
    Instead, we maintain a versioned mono-repo for all standard libraries, and expect external libraries to be bound to a version.
	This does *not* scale, but is reasonable at our current development team size.
- Formulating a coherent and complete specification for dependencies.
	We integrate dependency specification into Makefiles, and enable adapters between the Composite build system, and external libraries.
- Dependency minimization is not provided.
	If a component, interface, or library specifies a set of dependencies, Composite provides no means to ensure that set is minimal.
	As we use static libraries (`lib*.a` files), only `*.o` files that satisfy undefined symbols are included.
- Libc is special-cased in the system.
	We use `musl` libc, and ensure that it is compiled into components **after** all other libraries and interfaces are linked to.
	This is required to ensure that all necessary libc functionality is pulled in (i.e., that the linking processes sees the undefined symbols from other libraries and interfaces).

## Compilation Overview

There are six main `make` rules to guide system construction.

- `make config` -
	This is run once, and first.
	It selects the architecture.
- `make init` -
	This builds

	1. libraries that do *not* want to pay the cost of potential recompilation during the normal development cycle, compile here.
	2. libraries that have an initialization procedure that generate include files must be run before other libraries that depend on them.
- `make` or `make all` -
	The build command for the regular development cycle.
	Builds all libraries, interfaces, and then components, based on normal `make` build dependencies.
- `make clean` -
	Cleans and removes the compilation by-products of much of the implementation, aside from the state that was built in `init`.
- `make distclean` -
	Cleans all compilation by-products including those constructed by `init`.
- `make component`
	This is the main mechanism to create components that are statically linked with a specific set of libraries, and interface *variants*, and with a given set of initial arguments that are used to create a set of components in a specific system image.
	This rule is used by the automatic compilation process of the `composer` to construct a customized system image.

The Composite compilation process assumes that

- We do *not* use system includes (as we provide all relevant code), and do not use dynamic linking.
	Thus there should *not* be a used Procedure-Linkage-Table (PLT) in the executables.
- Position-Independent Code (PIC) is not used as we're currently statically linking all executables.
	Because of this, the Global Offset Table (GOT) should not be used in executables.
- Similarly, Position-Independent Executables (PIE) are not used.

References:

- PIC [introduction](https://en.wikipedia.org/wiki/Position-independent_code).
- Some [internals](https://wiki.gentoo.org/wiki/Hardened/Position_Independent_Code_internals).
