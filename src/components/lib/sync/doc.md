## sync

Synchronization facilities for components.

### Description

The synchronization abstractions include:

- Mutex locks for mutual exclusion.
    These currently do *not* support recursive (self) access.
- Semaphores.
    Nothing out of the ordinary here.
- Channels for buffered message passing.
    Properties include:

    - Currently, this only supports single producer, single consumer message passing, but locks can generalize.
	    In the future, we can invest in making multi-consumer.
	- Supports blocking and non-blocking APIs for "full" and "empty" conditions.
	- Only supports channels that are power-of-two sizes to avoid division on wraparound.
	- The channels are wait-free, thus can be used for cross-component communication with "somewhat complicated, but fine for many uses" trust relations.

- Blockpoints.
    These are the core abstraction for inter-core synchronization, blocking, and waking.
	At their core, they solve the "lost wakeup" problem, and define the object that tracks the waitqueue of threads.
	All of the previous abstractions are built directly on blockpoints.

### Usage and Assumptions

See the documentation prefixing each function.
