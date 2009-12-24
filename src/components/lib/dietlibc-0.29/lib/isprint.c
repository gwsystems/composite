
int __isprint_ascii ( int ch );
int __isprint_ascii ( int ch ) {
    return (unsigned int)(ch - ' ') < 127u - ' ';
}

int isprint ( int ch ) __attribute__((weak,alias("__isprint_ascii")));
