#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cobj_format.h>

#include "cFE_util.h"

void llprint(const char *s, int len)
{
	call_cap(PRINT_CAP_TEMP, (int)s, len, 0, 0);
}

int __attribute__((format(printf,1,2)))
printc(char *fmt, ...)
{
	  char s[128];
	  va_list arg_ptr;
	  int ret, len = 128;

	  va_start(arg_ptr, fmt);
	  ret = vsnprintf(s, len, fmt, arg_ptr);
	  va_end(arg_ptr);
	  llprint(s, ret);

	  return ret;
}

void panic_impl(const char* function, char* message){
	printc("cFE panic in %s: %s", function, message);
	assert(0);
}

void __isoc99_sscanf(void){
	PANIC("__isoc99_sscanf not implemented!");
}
