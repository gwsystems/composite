- Do a compilation of each component with libc and associated, necessary libraries to try and find all compilation errors.
    Then remove it, and re-link with mkimg.
	Note that removing it is optional, and if done, should be done by mkimg to enable `nm` and `objdump` introspection.
- Separate out a set of general configuration #define variables, and a set of component-specific #define variables that can specialize the component to its environment.
	Then enable these to be specified by the system specification, or derived from the graph.
	When compiling each separate component, enable these to be fed in to generate components that are specialized to their environment (e.g. static allocations of exactly the right size, thread pools of the right size based on hierarchy location, etc...)
- Change "at" to "target", and enable "targets" (note, plural)
- Update `cos_build_*` to `cos_build_sysspecname_releasename` to encode the build process.
- Might pass some set of default initargs to *each* component.
    These might include "name", ...
- Separate out the C defines for the namespacing assumptions on name mangling of stubs, and access them via the FFI to C to ensure they are consistent between the rust and C worlds (e.g. __cosrt_c_cosrtdefault, __cosrt_c_*, __cosrt_ucap*, __cosrt_s_*, __cosrt_s_cstub_*).
