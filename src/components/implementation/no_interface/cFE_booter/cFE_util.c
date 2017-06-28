#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cobj_format.h>

#include "gen/osapi.h"
#include "cFE_util.h"

void llprint(const char *s, int len)
{
	call_cap(PRINT_CAP_TEMP, (int)s, len, 0, 0);
}

int prints(char *s)
{
	int len  = strlen(s);
	llprint(s, len);
	return len;
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

void print_with_error_name(char* message, int32 error) {
	os_err_name_t local_name;
	OS_GetErrorName(error, &local_name);
	printc("%s, error %s\n", message, local_name);
}

void free(void* ptr) {
	PANIC("No free implementation!");
}

void* realloc(void* ptr, size_t new_size) {
	PANIC("No realloc implementation!");
	return NULL;
}

int __isoc99_sscanf(const char *buf, const char *fmt, ...) {
	int count;
	va_list ap;

	va_start(ap, fmt);
	count = vsscanf(buf, fmt, ap);
	va_end(ap);
	return count;
}
