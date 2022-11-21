#include <print.h>

/*
 * This overrides the weak symbol in `cos_component.c`, causing all
 * `printc`s to redirect here. You can do this so that we avoid
 * writing directly to serial without mutual exclusion, and can
 * instead write to this interface.
 */
int
cos_print_str(char *s, int len)
{
	int written = 0;

	while (written < len) {
		u32_t *s_ints = (u32_t *)&s[written];
		int ret;

		ret = print_str_chunk(s_ints[0], s_ints[1], s_ints[2], len - written);
		/* Bomb out. Can't use a print out here as we must avoid recursion. */
		if (ret < 0) written = *(int *)NULL;
		written += ret;
	}

	return written;
}
