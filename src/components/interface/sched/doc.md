## sched

Scheduler interface that enables thread creation, synchronization, and timing.

### Description

Components that implement this provide scheduling services, and implement some scheduling policy.
Most schedulers are going to depend on the `sl` and `sl_capmgr` libraries.

### Usage and Assumptions

- It is quite common for components to depend on both `sched` and `init` if they want the scheduler to initialize them, and to schedule them.
- Most scheduler implementations will also require you depend on the `sl` libraries, and the current software abstractions depend on a `capmgr` for thread creation.
