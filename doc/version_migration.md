=Migration of your code across versions=

==Commit 6dd6c6d70e8b67cc3669fdca315e873422933880==

*Missing `cg` component* - Removed the `cgraph.static` component.  In
your runscripts, if you have the cg component, and it is depended on
by more than boot, then you should make the dependency to
`no_interface.boot` instead.
