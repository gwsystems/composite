#ifndef SL_LOCK_H
#define SL_LOCK_H

#include <cos_kernel_api.h>

struct sl_lock {
    // will be 0 if no one holds the lock
    volatile thdid_t holder;
};

#define SL_LOCK_STATIC_INIT() (struct sl_lock) { .holder = 0 }

void sl_lock_init(struct sl_lock *lock);

thdid_t sl_lock_holder(struct sl_lock *lock);

void sl_lock_lock(struct sl_lock *lock);

int sl_lock_timed_lock(struct sl_lock *lock, microsec_t max_wait_time);

void sl_lock_unlock(struct sl_lock *lock);

#endif
