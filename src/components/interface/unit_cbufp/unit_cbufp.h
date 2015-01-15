#ifndef UNIT_CBUFP_H
#define UNIT_CBUFP_H

#include <cbuf.h>
#include <cbufp.h>

void unit_cbufp2buf(cbufp_t cbuf, int sz);
cbufp_t unit_cbufp_alloc(int sz);
void unit_cbufp_deref(cbufp_t cbuf, int sz);
int unit_cbufp_map_at(cbufp_t cbuf, int sz, spdid_t spdid, vaddr_t buf);
int unit_cbufp_unmap_at(cbufp_t cbuf, int sz, spdid_t spdid, vaddr_t buf);

#endif /* !UNIT_CBUFP_H */
