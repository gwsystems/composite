#include <schedinit.h>

int schedinit_child_intern(spdid_t c);

int schedinit_child(void)
{
	return schedinit_child_intern(0);
}
