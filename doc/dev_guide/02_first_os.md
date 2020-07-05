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
