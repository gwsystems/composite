# Creating and Executing your First OS using Composite

This chapter will introduce you to how to build and run your first OS in Composite.
As Composite is based on the component-based construction of OSes, we'll focus on how to "put the legos together", and compose a specialized system from a specific set of components.

It is necessary to understanding a number of concepts:

- **Components.**
	Components are a combination of a page table and a capability table.
	They are the unit of isolation in the system and contain and constrain all memory and resource access.
	Threads can be created in a component and will begin execution at a specified address (like `__start` in SysV).
	Synchronous invocations to a component also specify entry points for thread-migration-based invocations.
- **Dependencies.**
	Compilation dependencies are specified for components, interfaces, and libraries.
	Dependencies are on libraries and interfaces, and the build system will generate the transitive closure of the dependencies.
- **Interfaces.**
	Similar to interfaces in Java, Composite interfaces decouple functional signatures from the implementation, thus enabling polymorphism of the underlying implementation.
	They provide all of the logic to serialize and deserialize functional arguments, thus their implementations might rely on other libraries or interfaces.
	Interfaces can have multiple *variants* that might link a client to their functionality in different ways.
	This could mean different means of serializing arguments, or implementing some interface as a library!
- **Component invocation.**
	Synchronous invocation resources in a component's capability table enable it to invoke a specific functional entry point into another component via a thread migration-based invocation.
	This is the building block of coupling OS service to their clients, and providing system abstraction.
- **Constructors.**
	Components that are responsible for building other components.
	They construct the page and capability tables for components, set up the synchronous invocations between them, and initialize any components that depend on them for execution.
- **Capability Managers.**
	These components are in charge of managing the capability table (and often page-tables) of other components.
	As such, resources can be allocated, and shared (e.g., shared memory), and later revoked.
	These components replace the complex capability management in the kernel in most other microkernels.
- **Schedulers.**
	Perhaps the least surprising: these schedule threads.
	They manage timer interrupts (thus preemptions), create threads, and implement policies for switching between them to optimize for some goals.
	The default scheduler is fixed-priority round-robin.
- **Composer and composition scripts.**
	To create an executable system, specific components, related in specific ways must be "linked" together into a collection of components that each manage some set of resources and provide some abstraction.
	The *composer* takes a composition script specification of a given collection of components, and variants for the interfaces that they depend on, and export.
	It will generate a set of initialization arguments that enable constructors, capability managers, and schedulers to understand how they should manage other components (e.g., how to construct synchronous invocations).

## Creating an executable system

We'll use the `cos` script to simplify using Composite.
Our first OS is going to have a constructor, a capability manager, a scheduler, and a simple scheduler test.
Specifically, we'll use the following system (from `composition_scripts/sched.toml`):

``` toml
[system]
description = "Simplest system with both capability manager and scheduler, from unit_schedcomp.sh"

[[components]]
name = "booter"
img  = "no_interface.llbooter"
implements = [{interface = "init"}, {interface = "addr"}]
deps = [{srv = "kernel", interface = "init", variant = "kernel"}]
constructor = "kernel"

[[components]]
name = "capmgr"
img  = "capmgr.simple"
deps = [{srv = "booter", interface = "init"}, {srv = "booter", interface = "addr"}]
implements = [{interface = "capmgr"}, {interface = "init"}, {interface = "memmgr"}, {interface = "capmgr_create"}]
constructor = "booter"

[[components]]
name = "sched"
img  = "sched.root_fprr"
deps = [{srv = "capmgr", interface = "init"}, {srv = "capmgr", interface = "capmgr"}, {srv = "capmgr", interface = "memmgr"}]
implements = [{interface = "sched"}, {interface = "init"}]
constructor = "booter"

[[components]]
name = "schedtest"
img  = "tests.unit_schedcomp"
deps = [{srv = "sched", interface = "init"}, {srv = "sched", interface = "sched"}, {srv = "capmgr", interface = "capmgr_create"}]
constructor = "booter"
```

We are loading the `booter` which is a constructor, the `capmgr` which is the capability manager, the `sched` which is a fixed priority, round-robin scheduler, and a simple `schedtest` which is running a minimal preemption test.
The `deps`, `implements`, and `constructor` specifications encode the dependencies for all components, thus the structure of the system.

Lets go through the processes from downloading Composite, to booting and running the system:

```
$ git clone git@github.com:gwsystems/composite.git
$ git branch -f loader origin/loader
$ git checkout loader
```

We should have the `loader` branch at this point.

```
$ ./cos
Usage:  ./cos  init|build|reset|compose <script> <output name>|run <binary>
```

The `cos` shell script is going to walk us through the execution of the system.
The first print out for each `cos` command is the raw command it is going to execute.
You can use these commands to dive into the code appropriately.

First, lets initialize and build the system:

```
$ ./cos init
[cos executing] make -C src config init
...
$ ./cos build
[cos executing] make -C src all
...
```

This will compile the system using all of the default parameters.
You'll see some warnings for the c++ standard library, unfortunately.
Next, lets compose the specific OS we want to create:

```
$ ./cos compose composition_scripts/sched.toml test
[cos executing] src/composer/target/debug/compose composition_scripts/sched.toml test
...
System object generated:
        /home/gparmer/data/research/composite/system_binaries/cos_build-test/cos.img
```

This will run our linker that consumes the specific configuration script, and (as specified) generates a system image that packs together a kernel, constructor, and all of the components within that constructor.
Now that we have the system image, we can run it!

```
$ ./cos run sysudo stem_binaries/cos_build-test/cos.img
[cos executing] tools/run.sh system_binaries/cos_build-test/cos.img
...
(0,8,4) DBG:Test successful! Highest was scheduled only!
(0,8,4) DBG:Test successful! We swapped back and forth!
```

We are in the process of standardizing our unit testing frameworks, so please excuse our ad-hoc output.

## Understanding the system binaries

Now lets dive into our executables a little bit.
This is useful to understand what's happening in the system, and to be able to debug different parts of the system.
All files are created in the `system_binaries/cos_build-test/` directory that was created with that name due to the `test` argument to `./cos run`.

```
$ ls system_binaries/cos_build-test/
constructor  cos.img  global.booter  global.capmgr  global.sched  global.schedtest  kernel_compilation.log
```

The `cos.img` image is the kernel that has been linked with the constructor binary.
The kernel boots up, and creates the `constructor` binary as the first component.
The `kernel_compilation.log` file contains the kernel compilation output.
Look here if you see errors when you `compose`.

Next, you see a directory for each component in the system named after the `name =` variable names in the composition script.
They are named `global.name` as they are all in a "global" scope in the composition script.
Note that we don't currently support any other scopes.
Each of those directories includes compilation by-products:

```
$ ls system_binaries/cos_build-test/global.sched/
compilation.log  initargs.c  initargs.o  sched.root_fprr.global.sched
```

The binary is the executable for the scheduler.
You can introspect on that object using `objdump` and `nm`.
For example, see the Debugging section of this document to see how to map a faulty instruction back to the corresponding C.
This uses `objdump -Srhtl`.
The arguments passed into the component by the `composer` are in `initargs.c`.
We won't go over that format here.
Last, the `compilation.log` contains the compilation commands and output to aid in debugging.
