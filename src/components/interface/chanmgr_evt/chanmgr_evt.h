#ifndef CHANMGR_EVT_H
#define CHANMGR_EVT_H

#include <cos_component.h>
#include <evt.h>
#include <chan_types.h>

/***
 * This is a simple "add-on" API to the channel API (chan). If you're
 * also using event notifications and wish to add a channel to an
 * event, this is the way to do it!
 */

/**
 * Set and get the event resource id associated with a channel. There
 * are two event ids that can be associated with a channel, one that
 * will wake a receiver when a sender triggers the event (sends), and
 * the other for the sender to await a receiver to open a slot.
 *
 * - @id  - the channel
 * - @rid - the event resource id to add to the channel.
 * - @sender_to_reciever - is the event resource id we're referencing
 *     triggered by sender, and used by the receiver to block?
 * - @return - `chanmgr_evt_set`: `0` on success, `-n` for error
 *     `n`. `chanmgr_evt_get`: `n` is the event resource id, or `0` is
 *     error.
 */
int chanmgr_evt_set(chan_id_t id, evt_res_id_t rid, int sender_to_reciever);
evt_res_id_t chanmgr_evt_get(chan_id_t id, int sender_to_reciever);

/***/

#endif /* CHANMGR_EVT_H */
