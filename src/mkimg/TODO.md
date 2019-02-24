- Do a compilation of each component with libc and associated, necessary libraries to try and find all compilation errors.
    Then remove it, and re-link with mkimg.
	Note that removing it is optional, and if done, should be done by mkimg to enable `nm` and `objdump` introspection.
- Change "at" to "target", and enable "targets" (note, plural)
- Update `cos_build_*` to `cos_build_sysspecname_releasename` to encode the build process.
- Might pass some set of default initargs to *each* component.
    These might include "name", ...
