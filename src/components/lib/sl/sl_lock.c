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

	cycles_t deadline = sl_now() + sl_usec2cyc(max_wait_time), sleep_cycs = sl_usec2cyc(SL_LOCK_SLEEP_US);

	sl_cs_enter();
	while (lock->holder != 0 && sl_now() < deadline) {
		thdid_t curr_holder = lock->holder;

		sl_thd_yield_cs_exit(lock->holder);
		sl_cs_enter();

		/*
		 * Perhaps yielding to that thread did not work because that thread could be blocked? or is trying to take another lock..
		 * In which case, instead of wasting all the cpu cycles in this code, lets try a
		 nd sleep for a bit, so someone else gets to run
		 * and hope that the holder is woken up in the meantime and releases the lock..
		 *
		 * well, also don't want to sleep beyond the deadline (or timeout)..
		 */
		if (unlikely(lock->holder == curr_holder)) {
			cycles_t now;

			sl_cs_exit();
			now = sl_now();
			if (unlikely(now >= deadline - sleep_cycs)) sl_thd_block_timeout(0, now + sleep_cycs);
			sl_cs_enter();
		}
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

int
sl_lock_try_take(struct sl_lock *lock)
{
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
