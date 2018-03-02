#include <schedinit.h>

int schedinit_child_intern(spdid_t c, int u1, int u2, int u3, int *u4, int *u5);

int schedinit_child(void)
{
	int unused;

	return schedinit_child_intern(0, unused, unused, unused, &unused, &unused);
}
