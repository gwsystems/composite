## pongshmem

### Description

This interface provides a simple way to test shared memory between two components, allowing one to pass a reference to a shared memory region to the component that implements this interface. 


### Usage and Assumptions

A component can use this interface to allocate a shared memory region and pass the cbuf_t to the component that implements this interface, which should map the region to its address space using the cbuf identifier. This should be used to varify that shared memory can be used to communicate between two components.