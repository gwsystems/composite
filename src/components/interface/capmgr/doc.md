## capmgr

The capability manager API that provides dynamic resource allocation, delegation, and revocation.

### Description

A component that is an implementation of this interface is trusted to add and remove resources from the resource tables of components that depend on the `capmgr` interface.

### Usage and Assumptions

Any component that implements this, should also implement `capmgr_create` that enables even components that don't want to invoke any of the functions in this interface (as they are relatively static) to enable the `capmgr` to create threads within them (often in response to creation requests from a `sched`).
