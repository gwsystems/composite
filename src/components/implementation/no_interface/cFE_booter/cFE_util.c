#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cobj_format.h>
#include <llprint.h>

#include "gen/osapi.h"
#include "cFE_util.h"

void
llprint(const char *s, int len)
{
	call_cap(PRINT_CAP_TEMP, (int)s, len, 0, 0);
}

void
panic_impl(const char *function, char *message)
{
	printc("cFE panic in %s: %s\n", function, message);
	assert(0);
}

void
print_with_error_name(char *message, int32 error)
{
	os_err_name_t local_name;
	OS_GetErrorName(error, &local_name);
	printc("%s, error %s\n", message, local_name);
}
