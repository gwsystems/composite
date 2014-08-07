#ifndef UNIT_CBUFP_H
#define UNIT_CBUFP_H

#include <cbuf.h>
#include <cbufp.h>

void unit_cbufp2buf(cbufp_t cbuf, int sz);
cbufp_t unit_cbufp_alloc(int sz);
void unit_cbufp_deref(cbufp_t cbuf);

#endif /* !UNIT_CBUFP_H */
