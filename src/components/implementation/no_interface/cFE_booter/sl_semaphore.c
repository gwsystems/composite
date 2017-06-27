#include "sl_semaphore.h"

#include <ps.h>

struct sl_semaphore_waiter {
    thdid_t thdid;
    struct ps_list list;
};

void sl_semaphore_init(struct sl_semaphore *semaphore, int initial_count) {
    assert(semaphore);

    *semaphore = (struct sl_semaphore) {
        .count = initial_count,
        .epoch = 1
    };
    ps_list_head_init(&semaphore->waiters);
}


int sl_semaphore_is_held(struct sl_semaphore *semaphore) {
    assert(semaphore);

    sl_cs_enter();
    int no_waiters = ps_list_head_empty(&semaphore->waiters);
    sl_cs_exit();
    return !no_waiters;
}


int sl_semaphore_timeout_elapsed(microsec_t start_time, microsec_t timeout) {
    if (timeout == 0 || (sl_now_usec() - start_time) <= timeout) {
        return 0;
    }
    return 1;
}

int sl_semaphore_take_internal_no_cs(struct sl_semaphore *semaphore, microsec_t timeout) {
    assert(semaphore);

    struct sl_semaphore_waiter waiter = (struct sl_semaphore_waiter) {
        .thdid = sl_thd_curr_id()
    };
    ps_list_init(&waiter.list);

    microsec_t start_time = sl_now_usec();
    while (semaphore->count == 0 && !sl_semaphore_timeout_elapsed(start_time, timeout)) {
        ps_list_head_add(&semaphore->waiters, &waiter, list);
        sl_cs_exit();

        sl_thd_block(0);
        ps_list_rem(&waiter, list);

        sl_cs_enter();
    }

    int took_lock = 0;
    if (lock->holder == 0) {
        lock->holder = sl_thd_curr_id();
        took_lock = 1;
    }
    return took_lock;

}


void sl_semaphore_take(struct sl_lock *lock);
void sl_semaphore_take_no_cs(struct sl_lock *lock) {

}

int sl_semaphore_timed_take(struct sl_lock *lock, microsec_t timeout);
int sl_semaphore_timed_take_no_cs(struct sl_lock *lock, microsec_t timeout);

void sl_semaphore_release(struct sl_lock *lock);
void sl_semaphore_release_no_cs(struct sl_lock *lock);

void sl_semaphore_flush(struct sl_lock *lock);
void sl_semaphore_flush_no_cs(struct sl_lock *lock);
