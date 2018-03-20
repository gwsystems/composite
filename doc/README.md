# Composite Setup Guide

## Tools necessary for Composite
```
sudo apt-get -y install bc  
sudo apt-get -y install gcc-multilib  
sudo apt-get -y install binutils-dev  
sudo apt-get -y install qemu-kvm  
sudo apt-get -y install cmake  
sudo apt-get -y install build-essential  
sudo apt-get -y install xorriso  
sudo apt-get -y install curl  
sudo apt-get -y install python
```
If you want or need Rust on composite, also follow the steps in `rust_with_composite.md`
## Getting and building

We do all of our testing on 32 bit Ubuntu 14.04.
This is an old setup, and we need to update, but if you use another environment (i.e. a 64 bit system), your mileage will vary.

To get and compile the base system, you do the following once:

```
$ git clone https://github.com/gparmer/composite.git composite
$ cd composite/src/
$ make init
$ make
$ make run RUNSCRIPT=micro_boot.sh run      # microbenchmarks
$ make run RUNSCRIPT=unit_schedtests.sh run # simple scheduler tests
```

When developing:

```
$ make
$ make run RUNSCRIPT=micro_boot.sh run
```
Both `micro_boot.sh` and `unit_schedtests.sh` will launch a [QEMU](https://www.qemu.org/documentation/) instance, use `<Ctrl-a x>` to exit.
## The `doc` Directory

The only document that is currently up-to-date is the `style_guide/` (and `rust_with_composite.md`),
and this document. The others are *mostly* correct, but if they allude to Linux in any way, then ignore that part.

## Theory and abstractions

### Components

- Abstraction of protection, and functionality
- Isolated via page-tables
- components can export an interface of functions that other components can invoke (like a class)
  + in this way, they provide some software *abstraction* to other components

### Capability-based systems

- a set of "keys" that reference kernel resources
- having access to a key gives you access to the kernel resource
- each component has a set of keys, and the kernel provides means for modifying the set of keys a component has access to
- a row in the access control matrix
- Please read [this](http://www2.seas.gwu.edu/~gparmer/posts/2016-10-31-capability-based-systems.html)

### Resource tables

- an implementation of capability-based access control
- two resource tables: capability-table (to access abstract kernel resources such as threads), and page-tables (to access memory)
- need to be constructed by user-level
- quite tedious
- Please read [this](http://www2.seas.gwu.edu/~gparmer/posts/2016-04-06-capability-based-design.html)

### Threads

- Abstraction for computation
- Interrupts are converted into thread activation!

### Component Invocations

- a capability gives access to invoke a specific function in another component
- a thread executing in component A can invoke B
  + the *same thread* executes across the two components
  + but uses different stacks (isolation)
- a thread can *return* to A
- this mimics a function call
- each thread structure in the kernel tracks these invocations with an *invocation stack*
- please read [this](http://www2.seas.gwu.edu/~gparmer/posts/2016-01-17-composite-component-invocation.html)

### Scheduling

- scheduling policies defined in user-level components
- kernel provides a simple *dispatch* system call
  + ...and some other things ;-)
- schedulers schedule all threads in the system -- not just those that execute exclusively in the scheduler's protection domain

## Code structure

- `kernel/` is the generic parts of the kernel
- `platform/i386/` is the x86 boot and setup parts of the system
- `components/` is all of the component-specific stuff
  + `components/lib/` is a set of libraries that can be loaded into any component
  + `components/include/` is the include directory automatically imported by all components
  + `components/implementation/` is the directory that includes the implementation of all components
    - each sub-directory corresponds to an API, and each sub-sub-directory is a component
    - `tests` is a set of tests including both unit tests and micro-benchmarks
    - `no_interface` is a set of components that don't export an interface elsewhere...you'll focus mainly on these

## Libraries and the kernel API

- `cos_kernel_api.*`
    + hide the details of resource table construction
    + provide an easier-to use API (fewer strange arguments)
    + still esoteric
- `cos_defkernel_api.*`
    + make the assumption that this component is the "parent" for many created resources
    + scheduling receive end-points assume this component as the parent
    + use the tcap of this component by default
- `sl.*`
    + implement a scheduler creation framework
    + schedulers can be provided as "modules"
	    - scheduling policy modules
	    - timer interrupt policy modules
	    - different memory and resource allocation implementations
