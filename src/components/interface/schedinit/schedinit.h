#ifndef SCHEDINIT_H
#define SCHEDINIT_H

#include <cos_types.h>
#include <cos_component.h>

/* parent returns the shared memory initialized with ring-buffer for consuming notifications */
cbuf_t schedinit_child(void);

#endif /* SCHEDINIT_H */
