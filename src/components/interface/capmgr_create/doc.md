## capmgr_create

### Description

This interface does *not* provide significant functionality.
Its function should *not* be invoked, and if it is, it should return with no side-effects.

### Usage and Assumptions

This interface is used as a `composer` signal that a client of the interface can be managed by a specific capability manager.
This is necessary for the `composer` to create the permissions in the capmgr to be able to create a thread in the client of this interface.
`capmgr_create` to a `capmgr` is often paired with a `init` to a `sched` (that depends on the same `capmgr` for `capmgr_create` and `capmgr`).
