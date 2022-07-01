#ifndef TMRMGR_H
#define TMRMGR_H

/***
 * The timer API is like these found in FreeRTOS. Simply create a timer
 * (one-shot or periodic), associate it with an event, and wait on that
 * event. That's all.
 */

#include <cos_component.h>

/* All of these are the size of a word */
/**
 * The ID of the timer.
 */
typedef word_t tmr_id_t;

/**
 * The type of the timer. Set to 0 for periodic, set to 1 for one-shot.
 */
typedef enum {
	TMR_ONESHOT     = 0,
	TMR_PERIODIC 	= 1
} tmr_flags_t;

/**
 * Register a timer.
 *
 * - @usecs - The time to timeout in microseconds.
 * - @tmr_flags_t flags - The type of the timer.
 * - @return - The ID of the timer that have been created.
 *
 *     - `0` on success, and
 *     - `!0` if a timer cannot be created.
 */
 tmr_id_t tmrmgr_create(unsigned int usecs, tmr_flags_t flags);

/**
 * Teardown a timer.
 *
 * - @tmr_id_t id - The ID of the timer to teardown.
 * - @return - Unimplemented, always -1.
 *
 *     - `0` on success, and
 *     - `!0` if a timer cannot be deleted.
 */
 int tmrmgr_delete(tmr_id_t id);
 
 /**
 * Start a timer.
 *
 * - @tmr_id_t id - The ID of the timer to start.
 * - @return - Whether the operation is successful.
 *
 *     - `0` on success, and
 *     - `!0` if a timer cannot be started.
 */
 int tmrmgr_start(tmr_id_t id);
 
 /**
 * Stop a timer.
 *
 * - @tmr_id_t id - The ID of the timer to stop.
 * - @return - Unimplemented, always -1.
 *
 *     - `0` on success, and
 *     - `!0` if a timer cannot be stopped.
 */
 int tmrmgr_stop(tmr_id_t id);
 
/***
 * This is a simple "add-on" API to the timer API (chan). The timer API
 * is basically useless without this.
 */

/**
 * Set and get the event resource id associated with a timer. Only one ID
 * can be associated with the timer, and when the timer expires, the event 
 * will be fired.
 *
 * - @id  - the timer
 * - @rid - the timer resource id to add to the timer.
 * - @return - `tmrmgr_evt_set`: `0` on success, `-n` for error
 *     `n`. `tmrmgr_evt_get`: `n` is the timer resource id, or `0` is
 *     error.
 */
int tmrmgr_evt_set(tmr_id_t id, evt_res_id_t rid);
evt_res_id_t tmrmgr_evt_get(tmr_id_t id);

#endif /* TMRMGR_H */
