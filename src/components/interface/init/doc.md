## init

The interface used for component initialization.

### Description

Not only does this functionally enable the synchronization necessary for the dance between `cos_init`, `cos_parallel_init`, and `main` or `parallel_main`, it also is a *marker* used by the `composer` to enable the server providing this interface the resources and `initarg` values to be able to conduct this initialization.

### Usage and Assumptions

Any component that wishes to be initialized, *must* depend on this interface.
Generally, you should depend on the "most abstract" provider of the interface.
In a system that includes a constructor, a capability manager, and a scheduler, you should generally depend on the scheduler for `init`.
That way, you'll be scheduled using a traditional policy, and not non-preemptive FIFO (the policy for the constructor and capability manager).
Additionally, any capability manager must be enabled to create threads within a client of this interface, so an `init` to a `sched` should be paired with a `capmgr_create` to the `capmgr`.
