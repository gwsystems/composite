#ifndef COS_MEMCACHED_EXP_H
#define COS_MEMCACHED_EXP_H

#include "memcached/memcached.h"

void cos_mc_init_thd(LIBEVENT_THREAD *thd);
void cos_mc_establish_conn(void *arg);
void cos_mc_event_handler(const int fd, void *arg);
conn* cos_mc_get_conn(const int fd);

#endif /* COS_MEMCACHED_EXP_H */
