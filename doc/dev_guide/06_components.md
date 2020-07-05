# Components



## Execution Model

A component goes through a sequence of initialization functions when it starts up.
Whereas System V defines `__start` to execute constructors, then `main`, the initialization sequence in Composite must consider two additional constraints:

- The dependencies between components paired with the thread migration mean that there is a required initialization sequence.
	If two components $C$ and $S$ (client and server) have a dependency from $C \to S$, $S$ must initialize before the client.
	Were this not the case, the client, $C$, could invoke $S$ *before* it is initialized.
	Thus the initialization sequence must facilitate this synchronization.
- Service components must be able to leverage parallel execution.
	Whereas monolithic kernels provide APIs to create execution another core -- either passively through thread migration, or intentionally through by controlling affinity, the Composite kernel is far lower level, and such abstractions must be *created* via Component functionality.
	Thus, parallel execution is explicit through initialization, and services can execute on each core available to them.

![Execution begins at `__cosrt_upcall_entry` which kicks off the sequence of initialization functions for a component. Gray components run in parallel on each core, and blue run sequentially. A given function is only called if it is defined by your component, and the longest path is taken. If both `main` and `parallel_main` are defined, then only `parallel_main` is executed. Dashed transitions prevent client execution; only solid transitions allow clients to execute.](./resources/component_init.pdf)

Composite splits initialization into a variety of phases, and a component must *opt-in* to a specific initialization phase by defining the corresponding function.
Specifying a dependency on the `component` library is sufficient to enable this functionality.
The sequence of these calls is depicted in the Figure above.
We detail the functions below.

- `void cos_init(void);` - The first initialization function executed executes on the initialization core.
    When this returns, the system either continues execution with `cos_parallel_init`, or continues to one of the mains.
- `void cos_parallel_init(coreid_t cid, int init_core, int ncores);` - Each core on which the component executes has a thread that executes through this function.
	The id of the core `cid`, if the current invocation is the initial core (`init_core`), and the number of cores (`ncores`) are passed separately on each core.
	Threads barrier synchronize before this function is executed, so `cos_init` has completed execution by the time *any* thread executes this function.

After the previous functions have executed, the system will initialize this component's clients.
Thus, it is essential that all necessary initialization to receive invocations from client is completed before returning from the previous functions.

- `int main(void);` - This mainly exists to support legacy execution.
	The current system does *not* pass arguments, nor environmental variables.
	This function is called on the same care (in the same thread) as called `cos_init` and `cos_parallel_init(_, 1, _)`.
- `void parallel_main(coreid_t cid, int init_core, int ncores);` - Similar to `cos_parallel_init`, this is executed on each core, but done in parallel with client initialization and execution.
	For services that require persistent execution, this provides it.

## Abstractions - Execution

### Kernel API

### Scheduling Library

### Scheduler Component

## Abstractions - Kernel Resources

### Direct Capability Management

### Capability Manager

## Abstractions - Communication

### Synchronous Invocations

### Asynchronous Activations

### Channels

## RTOS

## Creating a New Component

All components exist in the `src/component/implementation/` directory.
To create a new component, we need to first answer which is the dominant interface it exports.
We either create, or make a new directory for that *interface*, and within it, create an appropriately-named directory for the component.
If it is an application, thus does *not* export an interface, then we use the `no_interface/` directory.
We can copy in the Makefiles and template from the `skel/` directory.
Luckily, a script will do all of this for us:

```sh
$ cd src/component/implementation/
$ sh mkcomponent.sh sched edf
```

Which would create a component in `src/component/implementation/sched/edf/` for us, and hook it into the build system.
