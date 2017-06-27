#ifndef SL_SEMAPHORE_H
#define SL_SEMAPHORE_H

#include <ps.h>

struct sl_semaphore {
    int count;
    int epoch;
    struct ps_list_head waiters;
};

void sl_semaphore_init(struct sl_semaphore *semaphore, int initial_count);

int sl_semaphore_is_held(struct sl_semaphore *semaphore);

void sl_semaphore_take(struct sl_lock *lock);
void sl_semaphore_take_no_cs(struct sl_lock *lock);

int sl_semaphore_timed_take(struct sl_lock *lock, microsec_t timeout);
int sl_semaphore_timed_take_no_cs(struct sl_lock *lock, microsec_t timeout);

void sl_semaphore_release(struct sl_lock *lock);
void sl_semaphore_release_no_cs(struct sl_lock *lock);

void sl_semaphore_flush(struct sl_lock *lock);
void sl_semaphore_flush_no_cs(struct sl_lock *lock);

#endif
