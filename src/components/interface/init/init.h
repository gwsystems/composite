#ifndef INIT_H
#define INIT_H

#include <cos_component.h>

void init_done(int cont); 	/* should we continue, or is execution done? */
void init_exit(int retval) __attribute__((noreturn));

#endif /* INIT_H */
