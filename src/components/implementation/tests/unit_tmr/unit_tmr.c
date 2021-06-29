#include <cos_component.h>
#include <llprint.h>
#include <evt.h>
#include <tmr.h>

/***
 * The timer test. You can set up periodic and one-shot timers here.
 */

#define TMR_ONESHOT_TIME (5000000U)
#define TMR_PERIODIC_TIME (2000000U)

/**
 * The timer event source. Here we'd only have one type of event.
 */
enum
{
	MY_TIMER_SRC_T = 1
};

int
main(void)
{
	struct tmr   t;
	struct evt   e;
	evt_res_id_t evt_id;

	printc("Call into timer manager to make a timer.\n");
	assert(tmr_init(&t, TMR_PERIODIC_TIME, TMR_PERIODIC) == 0);

	printc("Call into event manager to make a event.\n");
	assert(evt_init(&e, 2) == 0);

	/*
	 * Add the timer event to the event set, the associate the timer with that event ID so
	 * the timer manager knows which event to trigger when the timer expires.
	 */
	evt_id = evt_add(&e, MY_TIMER_SRC_T, (evt_res_data_t)&t);
	assert(evt_id != 0);
	assert(tmr_evt_associate(&t, evt_id) == 0);

	/* Start the timer */
	assert(tmr_start(&t) == 0);

	/* Event loop */
	while (1) {
		evt_res_data_t evtdata;
		evt_res_type_t evtsrc;
		if (evt_get(&e, EVT_WAIT_DEFAULT, &evtsrc, &evtdata)) return -EINVAL;
		assert(evtsrc == MY_TIMER_SRC_T);
		printc("Periodic timer fired.\n");
	}

	return 0;
}

void
cos_init(void)
{
	printc("Timer testbench init.\n");
}
