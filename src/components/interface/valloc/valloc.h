#ifndef   	VALLOC_H
#define   	VALLOC_H

/* Virtual address space allocation for a component */

void *valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages);
int valloc_free(spdid_t spdid, spdid_t dest, void *addr, unsigned long npages);
int valloc_alloc_at(spdid_t spdid, spdid_t dest, vaddr_t d_addr, unsigned long npages);

#endif 	    /* !VALLOC_H */
