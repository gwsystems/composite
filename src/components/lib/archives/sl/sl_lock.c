#include "sl.h"
#include "sl_lock.h"

void
sl_lock_init(struct sl_lock *lock)
{
    *lock = SL_LOCK_STATIC_INIT();
}

thdid_t
sl_lock_holder(struct sl_lock *lock)
{
    return lock->holder;
}

int
sl_lock_timed_take(struct sl_lock *lock, microsec_t max_wait_time)
{
    int result;
    cycles_t deadline = sl_now() + sl_usec2cyc(max_wait_time);

    sl_cs_enter();
    while (lock->holder != 0 && sl_now() < deadline) {
        sl_thd_yield_cs_exit(lock->holder);
        sl_cs_enter();
    }

    if (lock->holder == 0) {
        lock->holder = sl_thdid();
        result = 1;
    } else {
        result = 0;
    }
    sl_cs_exit();
    return result;
}

int sl_lock_try_take(struct sl_lock *lock) {
    int result;
    sl_cs_enter();
    if (lock->holder == 0) {
        lock->holder = sl_thdid();
        result = 1;
    } else {
        result = 0;
    }
    sl_cs_exit();
    return result;
}
