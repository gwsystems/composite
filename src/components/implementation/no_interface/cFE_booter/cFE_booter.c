#include <stdio.h>
#include <string.h>

#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>

static void
cos_llprint(char *s, int len)
{ call_cap(PRINT_CAP_TEMP, (int)s, len, 0, 0); }


int
prints(char *s)
{
	int len = strlen(s);

	cos_llprint(s, len);

	return len;
}

void cos_init(void)
{
    prints("We live!");
}
