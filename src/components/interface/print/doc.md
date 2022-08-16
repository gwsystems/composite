## print

An interface to override `printc` behavior to print to a component.

### Description

This interface overrides the weak symbol for `cos_print_str` with a version that invokes the `print_str_chunk` function in this interface.
That function is called to iteratively print out a string by invoking another component that serializes the output across cores.

See `lib.c` for where the print pipeline is hijacked.
