## chan

The main interface for accessing channel-based communication!

### Description

Creating, destroying, sending, and receiving over channels.
They are bounded-buffer, asynchronous-by default, blocking when empty/full channels.
Thus, they can be used for synchronization, and for data passing.
Eventually (not yet), they will also pass abstract resources to enable delegation.
If you need to communicate (and block) using multiple channels, look at `evt` to coordinate the concurrency.

### Usage and Assumptions

You *must* specify if you are going to use the channels for any communication pattern other than SPSC.
The `P` and `C` stand for `P`roducer and `C`onsumer, and the question is there is only a *single* producer or consumer, or if there can be *multiple* of them.
Initially, we'll only support SPSC: a fast implementation that avoids locks (thus avoids trust) by using a wait-free structure implemented in shared memory.
MP or MC can use a shared structure, but necessary trust is increased between communicating components.
