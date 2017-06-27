Booter
=========

There are two booter components in *Composite*, the low-level booter (`llboot`)
and the normal booter (`boot`).
Boot code is shared by all booter components from the source at
`src/components/implementation/no_interface/boot/booter.c`.
Each individual booter customizes the shared code by providing a
`boot_deps.h` file. 

Booting *Composite* consists of multiple steps. First, an image
containing all of the code and static data is loaded by the bootloader,
`cos_loader`. (So far, `cos_loader` runs from within Linux, but someday
*Composite* might be self-hosting.) This image contains the
*Composite* kernel, the `llboot` component, and an array of component objects
(`[cobjs]`). The following describes the boot process,
shared boot code, and individual booter components in more detail.

# Boot specialization `boot_deps.h`
The shared boot code is customized by each booter component by providing
the following dependencies through `boot_deps.h`:
* boot_deps: `boot_deps_init(), boot_deps_run(), comp_info_record()`
* synchronization: `LOCK(), UNLOCK()`
* printing: `printc()`
* scheduling: `__sched_create_thread_default()`
* memory management: `__local_mman_get_page(), __local_mman_alias_page()`

`boot_deps.h` also should declare any functions it calls from booter.c.

## Low-level booter: `llboot`
The entry point for `llboot` is `sched_init()`, which will call shared boot
code `cos_init()` the first time `llboot` is invoked from the `cos_loader`
on the initialization core. `cos_loader` invokes it a second time on the
init core after invoking it on the other cores, which create and start
initialization threads. When `cos_loader` invokes it the second time, it
will start the init core's initialization thread.
`llboot` loads (maps the memory for) base system components to satisfy booting,
thread scheduling, memory management, and printing. Usually, these will be the
`boot`, `fprr`, `mm`, and `print` components.
After `llboot` finishes, the initialization thread it created will proceed
backwards through the component graph executing `cos_init()` functions for
each of the base system components.

The `boot_deps.h` functions for `llboot`:
* `boot_deps_init()`: Creates an init thread (and a recovery thread).
* `boot_deps_run()`: Ensures that the executing CPU is the initialization core,
	and that there exists an initialization thread. Otherwise, does nothing.
* `comp_info_record()`: calls `boot_spd_set_symbs()` to setup the component.
* `LOCK()`: macro wrapper for `cos_sched_lock_take()`.
* `UNLOCK()`: macro wrapper for `cos_sched_lock_release()`.
* `printc()`: implemented like `printc` component.
* `__sched_create_thread_default()`: Does nothing.
* `__local_mman_get_page()`: Allocates the next page of contiguous
	virtual address. Sets the initial heap pointer to the page obtained
	by the first call of this function.
* `__local_mman_alias_page()`: Aliases memory at the page frame located at the
	 at the source address offset from the initial heap pointer.

## Normal booter: `boot`
The normal booter uses `cos_init` as its entry point. It is instantiated
and started by the `llboot` component.
`boot` creates the application and system components from the `[cobjs]`
it receives from `llboot`.

The `boot` component will create the next level
of components from those it finds in the cobjs it receives from `llboot`.

The `boot_deps.h` functions for `boot`:
* `boot_deps_init()`: Initializes the `spd_info_addresses` structure, which
	tracks the loaded components.
* `boot_deps_run()`: Does nothing.
* `comp_info_record()`: calls `boot_spd_set_symbs()` to setup the component. 
* `LOCK()`: macro wrapper for `sched_component_take()`.
* `UNLOCK()`: macro wrapper for `sched_component_release()`.
* `printc()`: provided by `printc` component.
* `__sched_create_thread_default()`: macro wrapper for `sched_create_thread_default`.
* `__local_mman_get_page()`: macro wrapper for `mman_get_page`.
* `__local_mman_alias_page()`: macro wrapper for `mman_alias_page`.

# Shared boot code: `booter.c`

The shared boot code procedure:

1. Runs `boot_deps_init()`.
2. Extracts the `[cobjs]` arguments from `cos_comp_info`, which is a structure
	comprising component information passed to the booter.
3. Expands the booter's virtual address space. Note that by default each
	component initially gets a single 4MB region for its
	virtual address space into which the component is unpacked. If more
	contiguous space is needed, it must be allocated during boot, because
	(presently) virtual address spaces are not permitted to overlap in
	*Composite*.
4. Unpacks the components from `[cobjs]`, for each one it:
	1. Allocates and maps a contiguous 4MB-aligned region. Each component
		gets a 4MB region by default, but if the component needs
		more then an additional 12MB will be allocated to it.
	2. Populates the component's memory into that region.
	3. Creates and activates capabilities.
5. Starts boot threads, if they exist.
6. Runs `boot_deps_run()`.

