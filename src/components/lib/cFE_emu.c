#include <llprint.h>

// FIXME: Query the cFE to decide whether printf is enabled
int is_printf_enabled = 1;

// FIXME: Point this at the real buffer size
#define OS_BUFFER_SIZE 4096

void OS_printf(const char *string, ...)
{
    if(is_printf_enabled) {
        char s[OS_BUFFER_SIZE];
        va_list arg_ptr;
        int ret, len = OS_BUFFER_SIZE;

        va_start(arg_ptr, string);
        ret = vsnprintf(s, len, string, arg_ptr);
        va_end(arg_ptr);
        cos_llprint(s, ret);
    }
}
