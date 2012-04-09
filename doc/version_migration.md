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

Commit e919be82b1856d776a7e7d1c4906611e4b388f4e
-----------------------------------------------

The order of dependencies matters for components.  If you want to
invoke the scheduler for anything, make sure that the scheduler comes
before the memory manager in the list of dependencies.  Else, you will
get BUG() calls from within the memory manager.  This should be taken
care of automatically in the cfuse compiler, for is manual for now.

This is an issue now, and not before as the base memory manager
exports the scheduler interface as well so that it can act as the base
scheduler interface.  However, none of its functions should ever be
called.
