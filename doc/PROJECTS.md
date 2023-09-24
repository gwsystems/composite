# TODO List of Small Projects

If anyone is interested in looking into any of these, we can create a github issue, and discuss there.

## Knowledge Domains

Each project is labeled with the knowledge domains necessary to make progress.
These labels follow.

- `components`: Can build/use Composite and write components.
- `design`: Understands the user-level design of core components.
- `kernel`: Can hack on the kernel.
- `build`: Can understand the build system.
- `composer`: Understand the composer.

## Projects

- `[build]` LSP directives for Components and Interfaces:
	Enable the build system to teach the Language Server Protocol (LSP) editor support the dependencies to use for auto-complete.
	See the integration of `compile_flags.txt` into the libraries for an example.
    Perform the same file generation for components and interfaces.
- `[build]` Remove the old-style C dependency creation with `-MMD -MP`.
	The build system still uses `sed` to generate dependencies.
- `[composer,build]` Enable the composer and build system to do a stack analysis of each component.
	Assembly analysis knowing the entry points.
- `[build,composer]` Generate a flame graph of the stack usage.
- `[build,composer]` Generate a control/function flow graph picture of each component (e.g. using `graphviz`) from the assembly analysis.
	Do the same for the kernel.
- `[build,composer]` Enable system composition to output a graphic representation of the system graph.
	Different versions of this output might include interfaces as intermediate nodes, and perhaps functions within those interfaces.
	Uses `graphviz` to output the graphic.
	Includes special annotations for scheduler, initializer, constructor, and capability manager.
	A separate representation might depict the capability and page-tables of components.
- `[components,design]` Design/implement protocol for resource delegation/revocation.
	The capability manager tracks delegations of kernel resources.
	If a delegation is performed, both components involved must agree to the operation.
	This requires a protocol for communicating between the three components to attest that the delegation is allowed.
	For example, one component, *S* wants to map a page into another, *C*.
	The capability manager must receive invocations from both *S* and *C* to establish that the delegation is consensual for both.
	One version of this protocol relies on the *same thread* to execute through *S* and *C*, thus establishing the linkage more easily.
	Another (more complicated) version enables delegation between different components and threads.
	Core challenges include avoiding DoS attacks on the server satisfying the requests, and managing trust between all three involved components.
	The protocol should be encoded in interfaces, and capable for use in low-level managers (capability manager), and higher-level ones as well (file-system).
- `[components,design]` Provide separate resource allocation *batteries*.
	The Patina [paper](https://www2.seas.gwu.edu/~gparmer/publications/rtas21patina.pdf) discusses a separate component that exists only to hold resources for a process, and to hand them out on demand to it.
	This is a good idea, and not something we have in Composite.
	Currently, the capability manager does allocation *and* delegation/revocation, but with resource battery components, it would only do delegation/revocation.
	The battery holds the resources, allocates them on demand, and uses the capability manager to share them with the client component(s).
- `[components,design,composer]` Enable multiple resource batteries.
	Instead of having a single resource battery for all client components, we want multiple, mapped in a configurable manner to components.
	This builds on the previous task, and adds logic to the Composer and Constructor to split up resources.
- `[kernel]` Replace current LAPIC code with X2APIC support.
	X2APIC is the long-term plan for x86_64, and is cleaner to support (uses MSRs instead of memory-mapped device accesses).
- `[kernel]` I/O APIC support port.
	Already supported in the slite PR from Phani.
	Needs to be ported over.
- `[kernel]` IO-MMU support.
	This is a larger lift, and requires associating devices with page-table contexts.
- `[kernel,build,components]` Get Arm7 working again.
	Was functional before the v4 kernel.
	Port over the previous support to the new system structure.
- `[kernel,build,components]` Risc-V support.
- `[kernel,build,components]` Armv8 (64 bit) support.
