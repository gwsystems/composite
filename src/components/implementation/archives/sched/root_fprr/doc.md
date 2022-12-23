## sched.root_fprr

This is the most common scheduler we use in Composite that implements fixed-priority, round-robin (within the same priority) scheduling.

### Description

Pulls most of the scheduling logic in from the `sl` and `sl_capmgr` libraries.

### Usage and Assumptions

- We assume that this will execute *on top of* the `capmgr`.
- All components that depend on this component for `init` will be scheduled by it.
- Components dependent on this for scheduling, *must* also depend on the capability manager for `capmgr_create` so that it is allowed to create threads in them.
