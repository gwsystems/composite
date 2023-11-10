# TODO List of Small Projects

If anyone is interested in looking into any of these, we can create a github issue, and discuss there.

## Knowledge Domains

Each project is labeled with the knowledge domains necessary to make progress.
These labels follow.

- `components`: Can build/use Composite and write components.
- `design`: Understands the user-level design of core components.
- `kernel`: Can hack on the kernel.
- `build`: Can understand the build system.
- `virt`: virtualization support.
- `binary`: Understand and operate on binaries, likely with [`pyelftools`](https://dev.to/icyphox/python-for-reverse-engineering-1-elf-binaries-1fo4) and [`capstone`](https://github.com/capstone-engine/capstone/blob/master/bindings/python/test_x86.py)
- `composer`: Understand the composer.

## Projects

- `[build]` LSP directives for Components and Interfaces:
	Enable the build system to teach the Language Server Protocol (LSP) editor support the dependencies to use for auto-complete.
	See the integration of `compile_flags.txt` into the libraries for an example.
    Perform the same file generation for components and interfaces.
- `[build]` Remove the old-style C dependency creation with `-MMD -MP`.
	The build system still uses `sed` to generate dependencies.
- `[composer,build,binary]` Perform a memory analysis of the component binaries to identify sources of memory consumption, and potential solutions.
	This might include symbols ranked by size, and sections ranked by size.
	This can be an analysis across all components, or each individual component.
	If it is an analysis across all components, it have an option to focus on like-symbols between components to understand if sharing could help reduce memory.
- `[composer,build,binary]` Enable the composer and build system to do a stack analysis of each component.
	Assembly analysis knowing the entry points.
- `[build,composer,binary]` Generate a flame graph of the stack usage.
- `[composer,build,binary]` Enable the composer and build system to do a control flow analysis of each component with the goal of doing a reachability analysis.
	Specifically, we want to do an assembly analysis in which we know component entry points, and can generate the set of interface dependency functions reachable by each.
- `[build,composer,binary]` Generate a control/function flow graph picture of each component (e.g. using `graphviz`) from the assembly analysis.
	Do the same for the kernel.
- `[build,composer,binary]` Generate a control/function flow graph picture of each component (e.g. using `graphviz`) from the assembly analysis demonstrating the mapping between exported interface functions, and depended on interface functions provided by depended-on components.
	The goal is to understand the flow of data through the component, which might inform how the shared memory passed into exported functions can be managed.
- `[build,composer,binary]` Enable system composition to output a graphic representation of the system graph.
	Different versions of this output might include interfaces as intermediate nodes, and perhaps functions within those interfaces.
	Uses `graphviz` to output the graphic.
	Includes special annotations for scheduler, initializer, constructor, and capability manager.
	A separate representation might depict the capability and page-tables of components.
- `[components]` It is useful and interesting to investigate the use of SIMD instructions and registers for data-movement.
 	SIMD registers (e.g. [AVX-2](https://en.wikipedia.org/wiki/Advanced_Vector_Extensions)) are quite large.
  	Compared to the normal registers (e.g. in total 16*8 = 128 bytes), the AVX-2 [registers](https://en.wikipedia.org/wiki/Advanced_Vector_Extensions#Advanced_Vector_Extensions) are a significant increase (e.g. 32*(256/8) = 1024 bytes), let alone AVX-512 (with 32*(512/8) = 2048 bytes).
  	We can use these registers to pass arguments/buffers via IPC!
  	This project will study the performance of SIMD operations, notably, populating them with buffers, populating buffers with their contents, and zeroing them out (researching the fastest way to do so).
  	Add SIMD message passing into an updated ping/pong set of IPC benchmarking components.
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
- `[virt]` Composite supports very minimal VMs in which Linux is configured in a specific manner.
	We'd like to create Linux images that work within these confines for applications we care about.
	We'd like to investigate two ways to do this:

	1. Use `buildroot` to set up generic images.
  		We'd want to see what a simple image, e.g. based on `nginx` requires from Linux, and its support, and validate that the image still runs in Composite.
    		Look into other applications (below).
    	2. Manually configure Linux (as we are now) using a simple `init`.
     		This is similar to Linux From Scratch.

	Applications we might be interested in include:

	- nginx
   	- memcached
   	- redis
   	- image processing applications
   	- ...
- `[virt]` We'd like VMs to interact with each other, and with the surrounding system, and Plan 9 is always cool.
  	Lets get the VMs speaking the 9p protocol to enable them to use filesystems outside of the VM, and using virtio 9p for the communication.
  	We likely want rust working in a component as the 9p server will be much easier in rust.
