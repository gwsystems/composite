#include <ctype.h>

int __isalnum_ascii ( int ch );
int __isalnum_ascii ( int ch ) {
    return (unsigned int)((ch | 0x20) - 'a') < 26u  ||
           (unsigned int)( ch         - '0') < 10u;
}

int isalnum ( int ch ) __attribute__((weak,alias("__isalnum_ascii")));
