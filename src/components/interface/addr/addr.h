/*
 * This API is really meant to only be used by capmgrs. It allows the
 * capmgr to get key addresses within another component they
 * oversee. For the time being, this is limited to the heap pointer
 * and capability frontier.
 */

#ifndef ADDR_H
#define ADDR_H

typedef enum {
	ADDR_HEAP_FRONTIER,
	ADDR_CAPTBL_FRONTIER,
	ADDR_SCB,
} addr_t;

unsigned long addr_get(compid_t id, addr_t type);

#endif /* ADDR_H */
