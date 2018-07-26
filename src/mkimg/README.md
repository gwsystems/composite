# Composite System Specification Processing and Image Generation

As Composite is a component-based system, it requires a specification that binds all involved components into a runtime image.
The Composite build, linking, and runtime can be summarized as such:

- `make` - compile all components with exported and depended on interfaces
- `mkimg <sysspec>` - take a specification of the runtime system including a subset of the components in `src/components/implementation/*/*`, and:
	- link them according to specified dependencies;
	- generate an executable component binary for each of them, and the information required to link them together via synchronous invocations; and
	- generate the image of the booter along with all of the component binaries and dependencies
- `booter` - when the system is booted, and the booter executes, it will load the components into separate address spaces, and start executing them

This program essentially captures the `mkimg` step, but also integrates closely with the `booter` to ensure that the components are correctly loaded.

# TODO

There is a relatively long list of things to add, and I'll add this essentially on-demand.
Some are lacking features, so they will be impossible until added, and others are hacks that can be worked around by specifically formulating the sysspec.
Some things are just completely broken.
They might manifest as annoying bugs in the near, or far future.

## Features

- Different capability images for different components.
- Integration of the hypercall interface and an implicit dependency on the booter.

## Hacks

- The interfaces are ignored when generating synchronous invocations.
	This means that I greedily try to match client functions with server implementations without paying heed to interface names.
	This can break the following behavior: Even when you don't want a specific server to provide a specific function to a client, it can if its exported interface has a namespace collision with another server that provides the specified interface.
	This requires that we refactor all interface functions such that for inteface `<if>`, all of its functions have the form `<if>_*`, and requires adding the logic in `compose.rs` to integrate this knowledge.

## Broken

- The error handling is completely jacked up (unfortunately).
	This was due to learning rust pains.
	Errors are currently returned as `String`s, which is broken.
	These must be converted into `Err`s.
