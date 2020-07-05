## capmgr.simple

A capability manager that focuses on simplicity.

### Description

The main area of simplification for this component is in statically limiting the number of delegations for each resource.
We can allocate a maximum, predefined, number of resources, and share them a maximum number of times.
Revocation will remove up to that many shared references.
This results in an implementation that is more amenable to analysis, but might be too limiting for larger systems.
It is somewhat surprising how far you can get with maximum three delegations, however in an RTOS.
The intuition here is that if we are mainly using channels for communication, they are most often one-to-one (SPSC), thus require sharing only between the channel manager, and two clients.

### Usage and Assumptions

- Assumes that the capability image upon initialization includes all resources that we'd have as if we were boot up on directly on the kernel.
    This includes resource table references to ourselves.
- Also assumes that `initargs` have been set up by the `composer` to tell us where the capabilities are that correspond to our clients.
- Assumes a maximum number of resources, and delegations for those resources.
