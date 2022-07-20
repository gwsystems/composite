#include <cos_component.h>
#include <llprint.h>
#include <consts.h>
#include <ps.h>
#include <sched.h>

#include <tmr.h>
#include <tmrmgr.h>
#include <static_slab.h>
#include <heap.h>
#include <cos_time.h>

#define MAX_NUM_TMR 32
#define MIN_USECS_LIMIT 1000

#undef TMR_TRACE_DEBUG
#ifdef TMR_TRACE_DEBUG
#define debug(format, ...) printc(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

struct tmr_info {
	unsigned int index;
	cycles_t timeout_cyc;
	unsigned int usecs;
	tmr_flags_t flags;
	evt_res_id_t evt_id;
};

SS_STATIC_SLAB(timer, struct tmr_info, MAX_NUM_TMR);
struct heap* timer_active;
unsigned int timer_heap[sizeof(struct heap) / sizeof(unsigned int) + MAX_NUM_TMR];
thdid_t main_thdid;
unsigned long modifying;

tmr_id_t
tmrmgr_create(unsigned int usecs, tmr_flags_t flags)
{
	tmr_id_t id;
	struct tmr_info* t;

	t = ss_timer_alloc();
	if (!t) return 0;

	id = ss_timer_id(t);

	t->usecs = usecs;
	t->flags = flags;
	t->timeout_cyc = 0;
	t->evt_id = 0;

	if (t->usecs < MIN_USECS_LIMIT) t->usecs = MIN_USECS_LIMIT;

	debug("Timer manager: timer created, id %d, usecs %d, flags %d\n", id, usecs, flags);

	ss_timer_activate(t);

	return id;
}

/**
 * Start the timer. The timer must be manually started after creation, no matter if it is
 * periodic or one-shot. One-shot timers can be started over and over again. Also, we may
 * not start a timer if it is not associated with a event.
 */
int
tmrmgr_start(tmr_id_t id)
{
	cycles_t wakeup;
	struct tmr_info *t;
	unsigned int lock;

	debug("Timer manager: timer start, id %d\n", id);
	t = ss_timer_get(id);
	if (!t) return -1;
	if (t->evt_id == 0) return -1;
	if (t->timeout_cyc != 0) return -1;

	lock = ps_faa(&modifying, 1);
	if (lock == 0) {
		t->timeout_cyc = time_now() + time_usec2cyc(t->usecs);
		assert(heap_add(timer_active, t) == 0);
		ps_faa(&modifying, -1);
	} else return -2;

	sched_thd_wakeup(main_thdid);

	return 0;
}

/**
 * Stop the timer. Nothing will happen if we try to stop a timer that is already stopped.
 */
int
tmrmgr_stop(tmr_id_t id)
{
	cycles_t wakeup;
	struct tmr_info *t;
	unsigned int lock;

	debug("Timer manager: timer stop, id %d\n", id);
	t = ss_timer_get(id);
	if (!t) return -1;
	if (t->timeout_cyc == 0) return -1;

	lock = ps_faa(&modifying, 1);
	if (lock == 0) {
		heap_remove(timer_active, t->index);
		t->timeout_cyc = 0;
		ps_faa(&modifying, -1);
	} else return -2;

	sched_thd_wakeup(main_thdid);

	return 0;
}

int
tmrmgr_delete(tmr_id_t id)
{
	/* TODO */
	return -1;
}

int
tmrmgr_evt_set(tmr_id_t id, evt_res_id_t rid)
{
	struct tmr_info *t;

	debug("Timer manager: timer event set, id %d, event %d\n", id, rid);
	t = ss_timer_get(id);
	if (!t) return -1;
	if (t->evt_id && rid != 0) return -1;

	t->evt_id = rid;

	return 0;
}

evt_res_id_t
tmrmgr_evt_get(tmr_id_t id)
{
	struct tmr_info *t;

	t = ss_timer_get(id);
	if (!t) return -1;

	return t->evt_id;
}

int
main(void)
{
	cycles_t wakeup;
	struct tmr_info *t;

	main_thdid=cos_thdid();
	printc("Timer manager: executing main with thread ID %lu.\n", main_thdid);

	/* Now we do a test around sched_thd_block_timeout and see... */
	while(1) {
		/*
		 * Are there any timers in queue? If there's none, we block indefinitely
		 * until someone registers a new timer. That guy will wake us up, so we can check
		 * whatever have happened. By default, we wakeup at least a once per second. The
		 * accuracy of the timer is at about a millisecond. If we got a periodic timer,
		 * we insert that guy into the queue repeatedly after its expire.
		 */
		if (modifying != 0) {
			wakeup = time_now() + time_usec2cyc(1000 * 1000);
			sched_thd_block_timeout(0, wakeup);
			continue;
		}

		wakeup = time_now();
		t = heap_peek((struct heap *)timer_heap);

		if (t != NULL) {
			/* At least one timer expired. Process all of them. */
			while(t->timeout_cyc <= (wakeup + time_usec2cyc(MIN_USECS_LIMIT))) {
				debug("Timer manager: id %d expired.\n", ss_timer_id(t));
				evt_trigger(t->evt_id);
				t = heap_highest((struct heap *)timer_heap);

				if (t->flags == TMR_PERIODIC) {
					debug("Timer manager: added back id %d to heap.\n", ss_timer_id(t));
					t->timeout_cyc = time_now() + time_usec2cyc(t->usecs);
					assert(heap_add((struct heap *)timer_heap, t) == 0);
				} else {
					t->timeout_cyc = 0;
				}

				t = heap_peek((struct heap *)timer_heap);
				if (t == NULL) break;
			}
		}

		if (t == NULL) {
			wakeup = time_now() + time_usec2cyc(1000 * 1000);
			sched_thd_block_timeout(0, wakeup);
			debug("Timer manager: idle-wakeup.\n");
		} else {
			wakeup = t->timeout_cyc;
			sched_thd_block_timeout(0, wakeup);
		}
	}

	return 0;
}

int
timer_cmp_fn(void* a, void* b)
{
	return ((struct tmr_info*)a)->timeout_cyc <= ((struct tmr_info*)b)->timeout_cyc;
}

void
timer_update_fn(void* e, int pos)
{
	((struct tmr_info*)e)->index = pos;
}

void
cos_init(void)
{
	printc("Timer manager: init.\n");

	/* Initialize active timer heap */
	modifying = 0;
	timer_active = (struct heap*)timer_heap;
	heap_init(timer_active, MAX_NUM_TMR, timer_cmp_fn, timer_update_fn);
}
