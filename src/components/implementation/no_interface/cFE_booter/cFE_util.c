#include <stdio.h>
#include <string.h>

#include <cos_component.h>
#include <cos_debug.h>
#include <cobj_format.h>

#include "gen/osapi.h"
#include "cFE_util.h"

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
