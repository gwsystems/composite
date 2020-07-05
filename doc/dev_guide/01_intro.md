# Introduction to Composite

This document provides a high-level overview of the Composite OS, and its software infrastructure.
Reading this should be sufficient to get a decent understanding how the software is hooked together, and how to productively develop an OS in Composite.
It is *not* sufficient to change any of the fundamental structure of the system, but quite a bit can be accomplished by staying within the default parameters of the system.

## Why a New OS?

Monolithic OSes have been amazing catalysts for the software progress we've seen since punch cards.
Most OSes are actually moving *away* from the traditional monolithic structure.
Linux continues to increase the number of drivers and services exported to user-level, and Android places a focus (through `binder`-based IPC) on user-level services; OSX still supports Mach APIs (a first generation microkernel) along-side its BSD personality; and Windows has been leaning into its NT design around user-level personality definition, for example, by integrating VMs into the system APIs (e.g., around the Linux subsystem).
[`DPDK`](https://www.dpdk.org/) and comparable library/driver linkages move I/O to user-level by *bypassing* the kernel wholesale.

Why is this?
What is happening here?
Different systems make those choices for different reasons, but there are number of consistent trends:

1. Moving functionality that doesn't need to be into the kernel into user-level increases isolation in the system.
    Less trustworthy services can be placed at user-level, and their failure will not impact the entire system.
1. Performance, especially when using kernel bypass, can be significantly higher when avoiding the general-purpose paths through the kernel.
    If the kernel has too much overhead, or if the overheads of system calls are significant, directly accessing I/O from user-level is the answer.
1. User-level definition of software enables the decoupling of languages and development environments.
	Higher-level functionalities (event OS services) can be implemented in higher-level languages, and using the full breadth of useful libraries provided by a modern development environment.
1. Modifying the kernel is hard.
    The kernel is a relatively adversarial development environment.
	Software complexity, concurrency, and parallelism are inescapable in this domain, and enabling user-level development of system services side-steps the legacy of kernel complexity.

It is also important to understand how the world has changed.
It is unlikely that desktop OSes are going to be replaced at any point.
Monolithic systems are simply a good fit for the large functional requirements of users.
However, in other domains, the requirements have been changing:

- *Servers: performance.*
    On the server side, network throughput is approaching that of DRAM, and latency has been precipitously falling, especially with direct RDMA access.
	The cost of network operations has made kernel overheads (even around the lowly system call) too much of a performance liability.
- *Servers: virtualization.*
	On the other hand, multi-tenant systems (e.g., computation for rent) require trusted virtualization facilities.
	Such systems do *not* require a complete monolithic OS to multiplex VMs, and in some ways the software complexity of a full OS is a liability.
- *Embedded and IoT systems.*
	On the other side of the spectrum, *small* systems that control the physical world around us are becoming more popular, powerful, and feature-rich.
	Traditionally these systems used a small Real-Time Operating System (RTOS) that focused on simplicity and reliability.
	Ironically, they were often more dependable while providing *zero* isolation between different tasks (they all ran co-located with the kernel).
	This makes sense for two reasons:

	1. as the total lines of code in the system were so small that a small development team could reasonably deploy a tested system, and
	1. the interface between software and the surrounding environment was narrowly defined, often around the sensor and actuator data that would traverse a few $I^2C$ lines.
		Without a user or a network adding complex interactions, the testing space was restricted.

    This has all changed, and the change is most evident in Autonomous Vehicles (AVs).
	Sensor processing is immensely complex (often using neural networks over many different video and radar feeds), actuators have multiplied and increased in complexity, and the planning of the system must consider many factors, and arrive at a decision within a bounded amount of time.
	Cars are now some of the most complex software ecosystems that exist -- akin to mobile data-centers.

	OSes must be more reliable, fundamentally predictable, and accommodate the consolidation of software that used to be spread across many different computational units, onto shared multi-core hardware.
	Embedded systems are often multi-tenant systems with much more stringent requirements around timing and reliability.

*Aren't microkernels failed technology?*
For quite a long time, the failure of microkernels has been a meme.
However, over the past 15 years, they have proven quite successful.
QNX is the underlying OS behind many reliable automotive systems, seL4 variants have driven the secure processor in many iPhone variants, and Minix manages all modern x86 chips (as "ring -3" firmware).
As microkernels focus on reliability and minimality, they are popular in security- and dependability-intensive domains that don't see much direct user interaction.
The seismic changes in the requirements for systems are only going to increase the trend toward microkernel-like systems.

### Why Composite?

Composite is a microkernel in the broadest sense.
It breaks OS software into separate page table protected protection domains, and uses IPC for coordination between them.
Device drivers and most traditional kernel functionality -- including networking, file systems, process management, etc... --  execute at user-level.

Composite focuses on the aspects of modern systems that are of increasing importance.
These include a pervasive design optimizing for the non-functional properties of the system, and the specialized construction of software systems (from the OS up) by component *composition*.

**Non-functional OS requirements.**
OSes have been designed primarily around providing a set of functionality (e.g., POSIX), and managing resources appropriately.
In contrast, Composite is designed to optimize non-functional properties including:

- *Predictable scalability.*
	As the number of cores increases to $N$, the cost of system operations should not increase.
	To *predictably* maintain a given cost independent of $N$.
	The goal: If an application can scale to effectively use $N$ cores, it should be able to!
	This intuitive goal is not provided by existing systems which can be prevented by scalability bottlenecks in the system.
	A few takes on how Composite provides this can be found in [Gadepalli et al. '20](https://www2.seas.gwu.edu/~gparmer/publications/rtas20slite.pdf), [Wang et al. '14](https://www2.seas.gwu.edu/~gparmer/publications/rtas14_fjos.pdf), [Wang et al. '15](https://www2.seas.gwu.edu/~gparmer/publications/rtas15speck.pdf), and [Wang et al. '16](https://www2.seas.gwu.edu/~gparmer/publications/eurosys16ps.pdf).
- *Resilience to failures.*
	Software failures are a reality, and must be planned for, and accommodated.
	In distributed systems, they are explicitly integrated into the design by leveraging redundancy.
	Composite aims to be resilient to failures by using pervasive isolation boundaries to isolate bodies of software, and system-level techniques to prevent fault propagation.
	We pioneered some techniques to micro-reboot portions of the system in response to failures very quickly (on the order of 50 microseconds).
	A few takes on how Composite provides this can be found in [Gadepalli et al. '19](https://www2.seas.gwu.edu/~gparmer/publications/rtas19chaos.pdf), [Gadepalli et al. '17](https://www2.seas.gwu.edu/~gparmer/publications/rtss17tcaps.pdf), [Pan et al. '18](https://www2.seas.gwu.edu/~gparmer/publications/rtas18mpu.pdf), [Song et al. '13](https://www2.seas.gwu.edu/~gparmer/publications/rtss13_c3.pdf), [Song et al. '15](https://www2.seas.gwu.edu/~gparmer/publications/rtas15cmon_extended.pdf), and [Song et al. '16](https://www2.seas.gwu.edu/~gparmer/publications/dsn16sg.pdf).
- *Security via the Principle of Least Privilege (POLP).*
	The Composite kernel is a *capability-based system* which means that all kernel resources are referenced by per-component tokens.
	This is the only way to address such resources, thus avoiding the [ambient authority](https://en.wikipedia.org/wiki/Ambient_authority) inherent in resource access through unrestricted namespaces (e.g., the file system).
	Components are *specialized* to a task, and their dependencies on other Components in the system are explicit, and controlled.
	Thus the impact of a compromise is heavily restricted to only those resources of the very specialized compromised component.
- *Predictable end-to-end execution.*
	Unfortunately, with the increased isolation that comes from raising hardware barriers between different system services and applications, the predictability of execution across multiple components is threatened.
	How many threads need to be scheduled across all components to enable the computation of a client's reply?
	Are there reasonable bounds on that latency?
	Composite takes a drastically different approach to scheduling and IPC based on thread migration and user-level scheduling to enable end-to-end predictable execution -- tight bounds can be placed on client service requests.

For a more extensive discussion of this, see the [position paper](https://www2.seas.gwu.edu/~gparmer/publications/ngoscps_19.pdf).

**OS design by composition.**
[Unikernels](https://en.wikipedia.org/wiki/Unikernel) enable the specialization of system code to a *specific* application, often avoiding system call overheads by going so far as to link the specialized kernel directly into the application.
When you specialize for a single application, this can make sense.
Composite is motivated by the same factors.
A specialized body of system software

- has a lower memory footprint,
- includes only necessary code thus reducing the system attack surface, and
- can be optimized by the compiler for the application's usage patterns (e.g., via partial evaluation and dead-code elimination).

However, Composite aims to enable the composition of a system for multiple applications.
In doing so, even the *isolation properties* of the system can be customized, providing strong VM-like isolation where required, and monolithic system-like service-sharing where extensive sharing is required.
Much of the focus of the Composite code-base is to decouple various aspects of system software into multiple components so that the composition of these components has maximum customizability.

## Interesting or Novel Aspects of Composite

This subsection will assume some knowledge about existing microkernels, and will conceptually place Composite into this ecosystem.

### Thread Migration-based IPC

Inter-Process Communication (IPC) is usually a focal point for microkernels.
Most modern (third generation) microkernels use *synchronous rendezvous between threads* to perform IPC.
With this, two threads, a *client* and a *server* synchronize to directly pass a limited number of arguments.
A sending client waits for a server to receive.
Once the server recieves, the client blocks on a receive.
The server computes a reply, and sends back to the client, which finally unblocks the client, sending it the reply.
In this way, it functionally mimics function call, despite being communication between isolated protection domains.
Using efficient primitives (`call` and `reply_and_wait`, this integration only requires two system calls, and two page table switches, which are the dominant costs in modern IPC systems.
I'll refer to this IPC mechanism as *traditional IPC*.

Composite instead uses *thread-migration-based invocations*^[Note that this is *not* related to the *cross-core thread migration* used by schedulers to move threads between cores over time.] between components for IPC.
The classical paper for this is [Ford et al.](https://www.usenix.org/legacy/publications/library/proceedings/sf94/full_papers/ford.pdf), though the *mechanisms* it uses are quite different from Composite's.
With thread-migration, the *same thread* executing in a client component *continues* execution in the server in which it invokes a function.
To ensure isolation between client and server, the *execution context* (e.g., registers, execution stack, and memory contents) are separated across components, but the *scheduling context* traverses components.
This effectively means that scheduling decisions are not required upon IPC, and that server component execution is properly accounted to the clients (even transitively), and scheduled as the clients.
For simplicity, I'll refer to this as *invocation IPC* or *Composite's IPC*.
The end-to-end (across many component invocations) proper accounting and scheduling of execution is *required* to enable predictable execution.

Ironically, most popular traditional IPC systems (modern L4 variants) have been moving toward thread migration-based IPC.
The complexities and sub-optimalities of doing so are summarized in [Parmer et al. '10](https://www2.seas.gwu.edu/~gparmer/publications/ospert10.pdf) which is old, but still just as valid.

Composite does not "pay" for using thread migration-based IPC.
Composite has the fastest round-trip IPC between protection domains that we know of.
We compared against seL4 in [Gadepalli et al. '19](https://www2.seas.gwu.edu/~gparmer/publications/rtas19chaos.pdf), which is known to have exceedingly fast IPC.

### User-level Scheduling

> TODO

The story of user-level scheduling in Composite is told in four parts:

- The details of how system level scheduling responsibilities over all threads in the system can be exported to user-level, isolated components that can be customized [Parmer et al. '08](https://www2.seas.gwu.edu/~gparmer/publications/parmer_west_rtss08.pdf),
- [Parmer et al. '10](https://www2.seas.gwu.edu/~gparmer/publications/rtas11_hires.pdf) provides details about how schedulers can be hierarchically composed to delegate scheduling duties closer to applications,
- [Gadepalli et al. '17](https://www2.seas.gwu.edu/~gparmer/publications/rtss17tcaps.pdf) discusses how *untrusting schedulers* can coordinate using temporal capabilities, and
- [Gadepalli et al. '20](https://www2.seas.gwu.edu/~gparmer/publications/rtas20slite.pdf) demonstrates how user-level scheduling can avoid kernel interactions to make Composite scheduling both capable of system-level scheduling, *and* faster than traditional kernel-resident schedulers.

### Wait-Free, Parallel Kernel

> TODO

The details of the Composite Kernel can be found in [Wang et al. '15](https://www2.seas.gwu.edu/~gparmer/publications/rtas15speck.pdf).

### Access Control for Time

> TODO

The details of Temporal Capabilities can be found in [Gadepalli et al. '17](https://www2.seas.gwu.edu/~gparmer/publications/rtss17tcaps.pdf).

### Minimal, Specialized OSes

> TODO

## Composite Development Philosophy

Composite explicitly separates software into four different types:

- The kernel (in `src/kernel/` and `src/platform/`) includes the ~7 KLoC^[KLoC = Thousand Lines of Code.] for the kernel.
    The rest of the Composite code executes at user-level.
- Component implementations (in `src/components/implementation/*/*/`) which include the main software for both system services, and applications.
- Interfaces (in `src/components/interface/*/`) which includes the prototypes for the functional interfaces that link together components, and the stub code which serializes and deserializes arguments for those invocations.
	Interfaces *decouple* the implementation from the specification, thus enabling the polymorphism required to enable the versatile composition of multiple components together (e.g., a client that depends on an interface, with a server that implements it in a specific way).
    These can include multiple *variants* which are different means of implementing the interface.
	This enables library-based implementations for an interface along-side separate service component-based implementations.
	The appropriate variant can be chosen based on the context of the calling (client) component.
- Libraries (in `src/components/lib/*/`) which can be Composite-specific libraries, and adapted external libraries.

### Kernel API

The Composite kernel provides a capability-based API.
The high-level ideas:

- A component is a combination of a page table and a capability table.
	A component (through its capability table) can have a capability to a capability table or page table (which we collectively call *resource tables*).
	That gives it the ability to modify (add, remove, change) the resource table.
	Thus, components that have these capabilities to resource tables of other components are entrusted to manage them.
- Threads are the abstraction for execution in the system, and begin execution in a component.
	A capability to a thread denotes the ability to switch to it.
	Schedulers are the components that require thread capabilities.
	Where a thread executes (in which component(s)) is unrelated to which component has thread capabilities (scheduler).
- Capabilities to synchronous invocations denote the ability of a component to *invoke* another.
	This is the basis for dependencies and enabling one component to harness the functionality of another.
	Importantly, these do *not* transfer much data (4 registers one way, and 3 back), and data transfer must be separately implemented (via interface logic).
- Capabilities to asynchronous activation end-points denote the ability to active a specific thread.
	However, they are meant to be used for event notification across trust boundaries.
	TCaps (temporal capabilities) are associated with threads in this case, and they include priority information that can be used to determine if the thread activation should cause a preemption of the current thread.
	Either way, a notification is added to the thread's scheduler that the activation has occurred.
	Unless implementing a device driver (which receives asynchronous activations from interrupt sources), or a scheduler, you can likely ignore this kernel resource.

### Dependencies

The *dependencies* for each of the bodies of user-level code are *explicit*.
Components, interfaces, and libraries each spell out the other libraries and interfaces they require for their functionality.
Composition scripts and the `composer` (in `src/composer/`) specify all of the software that should appear in a system image, all of the inter-component dependencies, and which interface variants should be used for each.

When implementing a component, it is important to ask which other libraries and interfaces you want to depend on, and ensure that those dependencies are explicit.
If you're implementing a system service, which interfaces do you want to export?
Make sure that your component's `Makefile` includes the proper values for this.

### Abstraction Hierarchy

The Composite kernel does *not* provide simple interfaces, nor high-level abstractions such as processes, file systems, memory management, and networking.
Those functionalities must be built up from components.
We're going through our third version of the user-level environment, thus not many high-level functionalities are currently provided.
I'll use the system bootup as an example of composing components to increase functionality.

- At system boot-time, only a *single* component is loaded and executed.
	Your component can be booted this way, in which case it can use the kernel API, and has capability access to all system resources!
	Generally, this is not all that useful.
	A good example of a component written in this style is `tests.kernel_tests` which can be executed through the `kernel_test.toml` composition script.
- If you want isolation between bodies of software, the kernel-loaded component can be the loader for other system components.
	We call a component that loads others a *constructor*.
	In this case, that component must be trusted to

	1. load in the memory for the components it is constructing,
	2. create a thread in each component (per core), and initialize the components correctly, and
	3. create synchronous invocation resources between components so that they can interact.

	Generally constructors are trusted to perform all operations to ensure the *control flow integrity* of its constructed components.
	In other words, the initialization entry point, and the synchronous invocation entry points are set up by the constructor, and no other component can alter thread entry points or flow of execution through the component.
	The goal is that the main logic of constructors should be less than 500 LoC, and, with libraries, < 4 KLoC.

	Components constructed this way, are executed FIFO, in accordance with the initialization procedure (see the chapter on the component execution model).
	This is generally not all that useful.
	An example of a system constructed in this style can be found in the composition script `ping_pong.toml`.
- When components wish to more dynamically access system resources (e.g., memory), we can add in a Capability and Memory Manager (which I'll shorthand as *capmgr*).
	These implement the `capmgr` and `memmgr` interfaces.
	The capmgr is responsible for receiving requests for system resources (new threads, memory allocations), and tracking those kernel resources.
	The capmgr is now in chart of initialization order of all components it is responsible for.
	Unfortunately, this means that those components are still executed FIFO!
	Also not all that useful: we can do dynamic resource allocation, but execution is still rather constrained.
	A capability manager can be found in `capmgr.simple`.
	The `capmgr_ping_pong.toml` composition script is an example of this.
- Finally, if we want configurable scheduling policies, we can add a scheduler into the system (that implements the `sched` interface).
	The constructor will initialize the capmgr, which will initialize the scheduler, which will then initialize *and schedule* all other components.
	Schedulers will often use timers to premptively schedule threads in accordance with some policy.
	An example scheduler is `sched.root_fprr` which can be run with the composition script `sched_ping_pong.toml`.

This is a simple example of how layers of abstraction can be added bit-by-bit until some functional and non-functional requirements are achieved.
Additionally, new interfaces that implement the specified interfaces could be implemented to provide the functionality in a specialized manner.

## Limitations and Bugs

We want to up-front with the current common limitations of the system.
First, you should know that if you see an error involving them, the general techniques to resolve them.
Second, you should know about them so that you don't make a design that assumes functionality where there is none.
Most of these are *not* systemic issues, and can be solved with enough engineering hours.

In no particular order:

- Composite has a challenging relationship with Thread Local Storage (TLS).
	Systems typically use a register to hold a pointer to a thread-specific pointer to their storage.
	On x86-32 this is the `%gs` segmentation register.
	Due to thread-migration, a single thread executes across components via invocations.
	We want to avoid having to lookup a per-thread, per-component value on each invocation, so services that are export interface should *not* rely on TLS.
	Applications should be able to use TLS, but currently there is no organized support.
	When a new thread is created, `cos_thd_mod(struct cos_compinfo *ci, thdcap_t tc, void *tlsaddr)` should be used to set the TLS register.
- By default, most Linux system calls (accessed through `musl` libc) will *not* work, and will report an error (look at the Linux references to translate the system call number into the functional system call).
	There is no default POSIX support in Composite, and POSIX support (outside of VMs) will never be a priority.
	However, restricted support for libc system calls can be added with the `posix` library.
	That library enables a component to define a set of library-defined system call implementations.
	This can be used, for example, to provide a tarball-defined RAM file system.
	Note that even pthreads system calls are not implemented in Composite.
	See the `sched` interface if you'd like to allocate, modify, and synchronize with threads, or the `sl` library if you are implementing a scheduler (as a library).
- Composite does not support general elf objects.
	To simplify the kernel, we assume that the program headers of the system component includes only two segments: one that is RO, and one that is RW.
	The Composite build system (notably `src/components/implementation/comp.ld`) will provide this invariant, but if you try and deviate from that linker, your mileage will vary.
