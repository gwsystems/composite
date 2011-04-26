#ifndef   	VALLOC_H
#define   	VALLOC_H

/* Virtual address space allocation for a component */

int valloc_init(spdid_t spdid);
void *valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages);
int valloc_free(spdid_t spdid, spdid_t dest, void *addr, unsigned long npages);

#endif 	    /* !VALLOC_H */
