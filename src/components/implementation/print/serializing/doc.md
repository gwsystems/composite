## print.serializing

A printing component that focuses on serializing printouts across cores to get more readable output for debugging, or in non-performance sensitive operations.

### Description

Print requests through `printc` and `prints` are buffered separately for each core until either a single `printc`/`prints`'s data has been flushed, or until it is preempted on this core, in which case, the printouts will be interleaved.
As such, concurrency on a single core will result in interleaving, but contention on the printout hardware (serial) across cores will avoid interleaving within a single printout.

### Usage and Assumptions

- Only 180 (or `PRINT_STRLEN`) bytes in a string is buffered at once.
    Thus printouts that are larger than this will result in multiple outputs.
- Output is buffered on a core until the length of the entire string is reached.
    Progress toward this is made by calling the interface multiple times.
	It is possible to misuse this buffering policy by changing this string length across calls.
	Thus, you should stick to the `printc`/`prints` APIs which do this properly.
