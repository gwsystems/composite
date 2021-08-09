## Chkpt

### Description
This component is a unit test for the baseline checkpoint functionality. This includes creating a checkpoint from a non-booter component (defined in `chkpt.c` in this case), creating a component from that checkpoint, and running it. The checkpoint copies the memory (we do not yet support the copying for dynamic allocations) and synchronous invocations from the initial component and allows the new component to skip initialization steps.

### Usage and Assumptions
- Assumes that the `chkpt.toml` runscript is used
- You must uncomment the `#define ENABLE_CHKPT` in `src/components/implementation/no_interface/llbooter/llbooter.c` for this test to work; checkpoint functionality is disabled by default in the system

