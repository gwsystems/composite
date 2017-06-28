#include "sl_lock.h"

#include "sl.h"

void
sl_lock_init(struct sl_lock *lock)
{
    assert(lock);
    *lock = SL_LOCK_STATIC_INIT();
}

thdid_t
sl_lock_holder(struct sl_lock *lock)
{
    assert(lock);
    return lock->holder;
}


void
sl_lock_lock(struct sl_lock *lock)
{
    assert(lock);

    sl_cs_enter();
    while (lock->holder != 0) {
        sl_thd_yield_cs_exit(lock->holder);
        sl_cs_enter();
    }
    lock->holder = sl_thdid();
    sl_cs_exit();
}

int
sl_lock_timed_lock(struct sl_lock *lock, microsec_t max_wait_time)
{
    int result;

    assert(lock);

    sl_cs_enter();
    cycles_t deadline = sl_now() + sl_usec2cyc(max_wait_time);
    while (lock->holder != 0 && sl_now() < deadline) {
        sl_thd_yield_cs_exit(lock->holder);
        sl_cs_enter();
    }

    if (lock->holder == 0) {
        lock->holder = sl_thdid();
        result = 1;
    }else {
        result = 0;
    }
    sl_cs_exit();
    return result;
}

void
sl_lock_unlock(struct sl_lock *lock)
{
    assert(lock);

    sl_cs_enter();
    assert(lock->holder);
    assert(lock->holder == sl_thdid());
    lock->holder = 0;
    sl_cs_exit();
}
