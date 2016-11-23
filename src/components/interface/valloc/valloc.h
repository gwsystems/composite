#ifndef   	VALLOC_H
#define   	VALLOC_H

/* Virtual address space allocation for a component */

void *valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages);
int valloc_free(spdid_t spdid, spdid_t dest, void *addr, unsigned long npages);

/*
 * Allocates memory at the address specified. Will use vas_mgr if necessary.
 * Returns 0 on success.
 */
int valloc_alloc_at(spdid_t spdid, spdid_t dest, void *d_addr, unsigned long npages);

#endif 	    /* !VALLOC_H */
