#ifndef   	LOCK_H
#define   	LOCK_H

#include <cos_synchronization.h>
unsigned long *lock_stats(spdid_t spdid, unsigned long *s);
int lock_stats_len(spdid_t spdid);

#endif 	    /* !LOCK_H */
