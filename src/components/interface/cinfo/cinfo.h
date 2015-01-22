#ifndef   	CINFO_H
#define   	CINFO_H

int cinfo_add(spdid_t spdid, spdid_t target, struct cos_component_information *ci);
void* cinfo_alloc_page(spdid_t spdid);
int cinfo_map(spdid_t spdid, vaddr_t map_addr, spdid_t target);
int cinfo_spdid(spdid_t spdid);

/* vaddr_t cinfo_map_peek(spdid_t spdid); */

#endif 	    /* !CINFO_H */
