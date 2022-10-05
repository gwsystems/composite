#ifndef PRINT_H
#define PRINT_H

#include <cos_component.h>
#include <cos_stubs.h>

/**
 * `print_str_chunk` prints out three chunks of a string, serialized
 * into three, four byte chunks. The main utility of this function is
 * that it should serialize output to the serial instead of printing
 * multiple core's data interleaved.
 *
 * The typical use of this will be to simply include this interface in
 * a component's dependencies, which will override the `cos_print_str`
 * function from `cos_component.c`, and direct all `printc` calls to
 * use this API.
 *
 * - `@chunkN` - The `N`th 4 byte chunk of the string.
 * - `@len` - the remaining length of the string which might extend
 *   beyond these chunks. After this `len` is written, the print
 *   should be flushed and output.
 * - `@return` - the number of bytes written out.
 */
int print_str_chunk(u32_t chunk0, u32_t chunk1, u32_t chunk2, int len);
int COS_STUB_DECL(print_str_chunk)(u32_t chunk0, u32_t chunk1, u32_t chunk2, int len);

#endif /* PRINT_H */
