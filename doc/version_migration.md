=Migration of your code across versions=

==Commit 6dd6c6d70e8b67cc3669fdca315e873422933880==

*Missing `cg` component* - Removed the `cgraph.static` component.  In
your runscripts, if you have the cg component, and it is depended on
by more than boot, then you should make the dependency to
`no_interface.boot` instead.

==Commit cb4bef52bbc729a7eaececa60fb59c3f4978fccc==

From the component lists *and* from the dependency lists in your
runscripts:

1. Remove `bc`.

2. Remove `st`.

3. Change dependency for `fprr` (scheduler) from `mm.o` to `[parent_]mm.o`.

4. Remove `cg`.

See examples in the `unit_*.sh` runscripts.
