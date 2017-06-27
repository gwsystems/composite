#ifndef SL_LOCK_H
#define SL_LOCK_H

#include <cos_defkernel_api.h>

struct sl_lock {
    // will be 0 if no one holds the lock
    thdid_t holder;
};

void sl_lock_init(struct sl_lock *lock);

thdid_t sl_lock_holder(struct sl_lock *lock);

void sl_lock_lock(struct sl_lock *lock);
void sl_lock_lock_no_cs(struct sl_lock *lock);

int sl_lock_timed_lock(struct sl_lock *lock, microsec_t timeout);
int sl_lock_timed_lock_no_cs(struct sl_lock *lock, microsec_t timeout);

void sl_lock_unlock(struct sl_lock *lock);
void sl_lock_unlock_no_cs(struct sl_lock *lock);

#endif
