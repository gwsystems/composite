#ifndef   	CINFO_H
#define   	CINFO_H

int cinfo_map(spdid_t spdid, vaddr_t map_addr, spdid_t target);
spdid_t cinfo_get_spdid(int iter);

/* vaddr_t cinfo_map_peek(spdid_t spdid); */

#endif 	    /* !CINFO_H */
