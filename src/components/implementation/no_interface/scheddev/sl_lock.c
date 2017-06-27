#include "sl_lock.h"

#include "sl.h"

void sl_lock_init(struct sl_lock *lock) {
    assert(lock);

    *lock = (struct sl_lock) {
        .holder = 0
    };
}

thdid_t sl_lock_holder(struct sl_lock *lock) {
    assert(lock);
    return lock->holder;
}

// If timeout is 0, ignore the timeout
int sl_lock_timeout_elapsed(microsec_t start_time, microsec_t timeout) {
    if (timeout == 0 || (sl_now_usec() - start_time) <= timeout) {
        return 0;
    }
    return 1;
}

int sl_lock_lock_internal_no_cs(struct sl_lock *lock, microsec_t timeout) {
    assert(lock);

    microsec_t start_time = sl_now_usec();
    while (lock->holder != 0 && !sl_lock_timeout_elapsed(start_time, timeout)) {
        thdid_t holder = lock->holder;

        /*
         * If we are preempted after the exit, and the holder is no longer holding
         * the critical section, then we will yield to them and possibly waste a
         * time-slice.  This will be fixed the next iteration, as we will see an
         * updated value of the holder, but we essentially lose a timeslice in the
         * worst case.  From a real-time perspective, this is bad, but we're erring
         * on simplicity here.
         */
        sl_cs_exit();
        sl_thd_yield(holder);
        sl_cs_enter();
    }

    int took_lock = 0;
    if (lock->holder == 0) {
        lock->holder = sl_thd_curr_id();
        took_lock = 1;
    }
    return took_lock;
}


int sl_lock_lock_internal(struct sl_lock *lock, microsec_t timeout) {
    sl_cs_enter();
    int result = sl_lock_lock_internal_no_cs(lock, timeout);
    sl_cs_exit();
    return result;
}

void sl_lock_lock(struct sl_lock *lock) {
    assert(lock);

    // Lock internally, which should always succeed since timeout is 0
    assert(sl_lock_lock_internal(lock, 0) == 1);
}

void sl_lock_lock_no_cs(struct sl_lock *lock) {
    assert(lock);

    // Lock internally, which should always succeed since timeout is 0
    assert(sl_lock_lock_internal_no_cs(lock, 0) == 1);
}


int sl_lock_timed_lock(struct sl_lock *lock, microsec_t timeout) {
    assert(lock);

    if (timeout == 0) {
        return 0;
    }
    return sl_lock_lock_internal(lock, timeout);
}

int sl_lock_timed_lock_no_cs(struct sl_lock *lock, microsec_t timeout) {
    assert(lock);

    if (timeout == 0) {
        return 0;
    }
    return sl_lock_lock_internal_no_cs(lock, timeout);
}


void sl_lock_unlock(struct sl_lock *lock) {
    assert(lock);
    sl_cs_enter();

    sl_lock_unlock_no_cs(lock);

    sl_cs_exit();
}

void sl_lock_unlock_no_cs(struct sl_lock *lock) {
    assert(lock);

    assert(lock->holder == sl_thd_curr_id());
    lock->holder = 0;
}
