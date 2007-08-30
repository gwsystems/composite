#include <ctype.h>

int __isalpha_ascii ( int ch );
int __isalpha_ascii ( int ch ) {
    return (unsigned int)((ch | 0x20) - 'a') < 26u;
}

int isalpha ( int ch ) __attribute__((weak,alias("__isalpha_ascii")));
