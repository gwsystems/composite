#ifndef   	PERIODIC_WAKE_H
#define   	PERIODIC_WAKE_H

int periodic_wake_create(spdid_t spdinv, unsigned int period);
int periodic_wake_remove(spdid_t spdinv, unsigned short int tid);
int periodic_wake_wait(spdid_t spdinv);

#endif 	    /* !PERIODIC_WAKE_H */
