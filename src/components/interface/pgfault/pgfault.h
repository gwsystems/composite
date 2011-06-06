#ifndef   	PGFAULT_H
#define   	PGFAULT_H

int fault_page_fault_handler(spdid_t spdid, void *fault_addr, int flags, void *ip);

#endif 	    /* !PGFAULT_H */
