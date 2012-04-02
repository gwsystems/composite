Migration of your code across versions
======================================

Commit 6dd6c6d70e8b67cc3669fdca315e873422933880
-----------------------------------------------

**Missing `cg` component** - Removed the `cgraph.static` component.  In
your runscripts, if you have the cg component, and it is depended on
by more than boot, then you should make the dependency to
`no_interface.boot` instead.

Commit cb4bef52bbc729a7eaececa60fb59c3f4978fccc
-----------------------------------------------

From the component lists *and* from the dependency lists in your
runscripts:

1. Remove `bc`.

2. Remove `st`.

3. Change dependency for `fprr` (scheduler) from `mm.o` to `[parent_]mm.o`.

4. Remove `cg`.

See examples in the `unit_*.sh` runscripts.

Commit 8a7ffc887d7a795185079e0977ec0f217d392118
-----------------------------------------------

Scheduling of boot-time initialization threads.  No component changes,
but now

1. The priorities and threads given in the runscript are now ignored.
If you want an initialization thread, depend on the scheduler.  If you
want to be scheduled for initialization before another component, then
appear earlier in the list of components.

2. If you want to have a thread with a specific priority, use
sched_create_thread to create it.
