#ifndef   	PERIODIC_WAKE_H
#define   	PERIODIC_WAKE_H

int periodic_wake_create(spdid_t spdinv, unsigned int period);
int periodic_wake_remove(spdid_t spdinv, unsigned short int tid);
int periodic_wake_wait(spdid_t spdinv);
int periodic_wake_get_misses(unsigned short int tid);
int periodic_wake_get_deadlines(unsigned short int tid);
long periodic_wake_get_lateness(unsigned short int tid);
long periodic_wake_get_miss_lateness(unsigned short int tid);
int periodic_wake_get_period(unsigned short int tid);

#endif 	    /* !PERIODIC_WAKE_H */
