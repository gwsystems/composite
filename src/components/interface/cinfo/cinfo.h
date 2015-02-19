#ifndef   	CINFO_H
#define   	CINFO_H

int cinfo_add_heap_pointer(spdid_t spdid, spdid_t target, void *hp);
void* cinfo_get_heap_pointer(spdid_t spdid, spdid_t target);

#endif 	    /* !CINFO_H */
