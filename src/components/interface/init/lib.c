#include <cos_config.h>

/*
 * This is implemented as a library for all variant's use. Library
 * functions (in this case public-facing APIs, but often just utility
 * functions) are found in the c files in this directory. They
 * generate a .a static library that is linked on-demand.
 */
int
init_parallelism(void)
{
	return NUM_CPU;
}
