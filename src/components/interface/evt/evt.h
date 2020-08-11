#ifndef EVT_H
#define EVT_H

/***
 * The event API is the analog of `epoll` or `kqueue` (similarly
 * traditional `select` or `poll`). It enables a thread to await an
 * event on any number of resources (instead of blocking waiting for a
 * specific resource while ignoring the rest). It is *event
 * triggered*, which is why I compare to `epoll` and `kqueue`. This
 * means that once it tells you that a resource has a pending event,
 * you *must* flush all of those pending events as they will not be
 * re-reported. It tells you when a resource mode-changes from "not
 * available" to "available". For example, it notifies you when data
 * becomes available in a channel, when a timer has triggered, or when
 * a lock becomes available. However, it does *not* tell you *if* they
 * are *currently* available (level-triggered). Thus, this API is
 * often paired with non-blocking interfaces for the resources so
 * that, for example, a channel can be cleared out without blocking on
 * it, before awaiting for it using the event API again.
 *
 * A typical event loop looks something like:
 *
 * ```c
 * enum {
 *     MY_CHAN_SRC_T  = 1,
 *     MY_TIMER_SRC_T = 2,
 *     MY_LOCK_SRC_T  = 3,
 *     // ...
 * }
 * struct chan *c = chan_alloc(&c);
 * struct evt e;
 * evt_init(&e);
 * chan_evt_add(c, e, MY_CHAN_SRC_T, (evt_data_t)c);
 * // ...
 *
 * while (1) {
 *     evt_data_t evtdata;
 *     evt_src_t  evtsrc;
 *     if (evt_wait(&e, &evtsrc, &evtdata)) return -EINVAL;
 *
 *     switch (evtsrc) {
 *     case MY_CHAN_SRC_T: {
 *         channel_handler((struct chan *)evtdata); // clear out the channel
 *         break;
 *     }
 *     // ...
 *     default: err_panic("event source unknown")
 *     }
 * }
 * ```
 *
 * Note that if you don't want to trust the event component for your
 * own integrity, then you should not (as above) create the channel
 * source from the pointer. An event component could return an
 * arbitrary pointer and cause trivial faults and/or corruption. This
 * this, in the general case, an index should be passed, and used to
 * look up the channel. You can use the static_alloc.h API for this,
 * for instance.
 */

#include <cos_component.h>

/* All of these are the size of a word */

/**
 * The type of the resource (channel, mutex, timer, etc...) as defined
 * by the client.
 */
typedef word_t evt_res_type_t;
/**
 * The data associated with the resource. This is passed when
 * associating a resource with an event channel, and when an event
 * notification is provided, the data is also provided. Often used to
 * find the object that had the event. Can be a pointer, or an id to
 * perform a lookup.
 */
typedef word_t evt_res_data_t;
/**
 * The opaque id associated with the resource that is used to trigger
 * events.
 */
typedef word_t evt_res_id_t;

typedef enum {
	EVT_WAIT_DEFAULT     = 0,
	EVT_WAIT_NONBLOCKING = 1
} evt_wait_flags_t;

#include <evt_private.h>

/**
 * Initialize an event resource. Initialization does *not* allocate
 * memory, and only initializes an existing structure.
 *
 * - @evt - The event structure
 * - @max_evts - the maximum number of event sources that can be added
 *               to the event.
 * - @return -
 *
 *     - `0` on success, and
 *     - `!0` if an event channel cannot be created.
 */
int evt_init(struct evt *evt, unsigned long max_evts);

/**
 * Teardown an event resource. Teardown does *not* free
 * memory, and only removes the backing resources.
 *
 * - @evt - The event structure
 * - @return -
 *
 *     - `0` on success, and
 *     - `!0` if the event channel still has resources associated with it.
 */
int evt_teardown(struct evt *evt);

/**
 * Get the next event from the event resource.
 *
 * - @evt - the event
 * - @flags - options for retrieving the event
 * - @return src - returns the event source type
 * - @return ret_data - the data associated with the resource
 * - @return -
 *
 *     - `0` if data is successfully returned,
 *     - `> 0` if nonblocking, and data is not available
 *     - `< 0` if there is an error, interpret as -errno
 */
int evt_get(struct evt *evt, evt_wait_flags_t flags, evt_res_type_t *src, evt_res_data_t *ret_data);

/**
 * Client API for generating and using `evt_res_id_t`s. This is the
 * *second* resource provided by the event manager. Each of these
 * identifiers uniquely identifies a resource associated with an event
 * channel. This includes a function to *allocate* a new
 * `evt_res_id_t` while adding it to an event. Also a function to
 * remove a resource from event's notification set, and for triggering
 * an event.
 *
 * `evt_add` allocates a resource id and adds it to an event
 * channel. When it is triggered, it will return the `srctype`, and a
 * resource `ret_data` values passed in. The trigger function triggers
 * a specific `rid`. Note that the client triggering an event is not a
 * default, and the common behavior is often that a manager will be
 * triggering a resource.
 *
 * - @e - the event channel operated on
 * - @rid - the resource being allocated and added/removed/triggered.
 * - @srctype - the type of the resource (client-defined)
 * - @ret_data - the data associated with the resource (client-defined)
 * - @return - For `evt_rem` and `evt_trigger`:
 *
 *     - `0` on success
 *     - `<0` on error (with an `-errno` value)
 *
 *     For `evt_add`:
 *
 *     - `0` on failure, and
 *     - `>0` is the resource id
 */
evt_res_id_t evt_add(struct evt *e, evt_res_type_t srctype, evt_res_data_t ret_data);
int evt_rem(struct evt *e, evt_res_id_t rid);

int evt_trigger(evt_res_id_t rid);

#endif /* EVT_H */
