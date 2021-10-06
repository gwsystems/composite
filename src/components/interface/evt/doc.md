## evt

Event notification API.
Traditional event APIs such as `select`, `poll`, `epoll`, and `kqueues` aggregate multiple event sources (sockets, files, pipes, timers, signals, etc...), and enable an application to wait for an event on any one of them.
This API provides a generic mechanism for resource managers (e.g., for channels or timers) to *trigger* generic events, and for applications to *wait* for any of those events.
The API at its core is very simple:

- create (and destroy) event end-points,
- add generic event sources to them,
- wait for an event on any of them, and
- have resource managers trigger events.

Any complexity in the implementation comes from attempting to optimize in a number of dimensions:

- We want to batch notifications to the maximum degree possible, and enable a *waiting* thread from even invoking the manager if there are pending events.
- For events that can be triggered without involving the corresponding resource manager (e.g., channels), we'd like to avoid invoking the manager to correspondingly trigger the event.

Note that channels are designed to provide both of these optimizations, thus the merging of the `evt` API with channels.

### Description



### Usage and Assumptions
