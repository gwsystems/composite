#ifndef SL_LOCK_H
#define SL_LOCK_H

#include <cos_kernel_api.h>
#include <sl.h>

struct sl_lock {
	// will be 0 if no one holds the lock
	volatile thdid_t holder;
};

#define SL_LOCK_STATIC_INIT() \
	(struct sl_lock) { .holder = 0 }

void sl_lock_init(struct sl_lock *lock);

thdid_t sl_lock_holder(struct sl_lock *lock);

static inline void
sl_lock_take(struct sl_lock *lock)
{
	sl_cs_enter();
	while (lock->holder != 0) {
		sl_thd_yield_cs_exit(lock->holder);
		sl_cs_enter();
	}
	lock->holder = sl_thdid();
	sl_cs_exit();
}

int sl_lock_timed_take(struct sl_lock *lock, microsec_t max_wait_time);

int sl_lock_try_take(struct sl_lock *lock);

static inline void
sl_lock_release(struct sl_lock *lock)
{
	sl_cs_enter();
	assert(lock->holder == sl_thdid());
	lock->holder = 0;
	sl_cs_exit();
}


#endif
