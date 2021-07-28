## `slm` Tests

The simple scheduler with unit tests for the `slm` library.

### Description

This is a simple example of putting together the facilities of the minimal scheduling library (`slm`) into a scheduler.
Currently uses static allocation, fixed priority round-robin, and quantum-based timeouts.

### Usage and Assumptions

The `slm_test.toml` runscript can be used to run the unit tests.

Some assumptions/TODOs:

- single core only
- add tests for asynchronous invocations and activations
- assumes quantums of 10ms
- likely will not work in multiplexing virtual environments (due to timing irregularities)
