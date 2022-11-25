#ifndef CHANMGR_CREATE_H
#define CHANMGR_CREATE_H

#include <cos_component.h>
#include <memmgr.h>
#include <chan_types.h>
#include <sync_blkpt.h>

/***
 * The interface to communicate with the channel manager. The
 * channel's resources are
 *
 * 1. the memory (potentially shared) to contain the channel's data,
 *    and
 * 2. the blockpoints to enable blocking synchronization on channel
 *    full and empty. Please note that using *blocking* APIs implies
 *    *trust* between communicating threads in that one could block
 *    the other indefinitely.
 *
 * The logic for the channel should be external to these resources.
 */

/**
 * Create the new channel, and return its id.
 *
 * - @args - see chan.h for a description
 * - @return - the channel id, or `0` on error.
 */
chan_id_t chanmgr_create(unsigned int item_sz, unsigned int slots, chan_flags_t flags);

/**
 * Two functions to get the *resources* associated with the allocated
 * channel. These resources include the blockpoints used for
 * inter-thread synchronization, and the (shared) memory backing the
 * channel.
 *
 * Correspondingly, this yields two functions: 1. to get the
 * blockpoints for the channel, and 2. to map in the backing memory
 * for the channel. Note that this can be invoked without a preceding
 * `chanmgr_create` if the component has reason to believe that it
 * already has access to a channel at a specified id (capability).
 *
 * - @id           - the channel for which we want to get the resources
 * - @return full  - the blockpoint for the full condition or `NULL` if
 *   we don't have access.
 * - @return empty - same, but for the empty condition.
 * - @return cb    - the composite buffer that can reference/share the memory
 * - @return mem   - the memory itself
 * - @return       - `0` = success, or `-1` otherwise (incorrect `id`).
 */
int chanmgr_sync_resources(chan_id_t id, sched_blkpt_id_t *full, sched_blkpt_id_t *empty);
int chanmgr_mem_resources(chan_id_t id, cbuf_t *cb, void **mem);

/**
 * Delete an existing channel in a component, and dereference it if it
 * is accessed from within multiple components.
 *
 * - @id - the channel id.
 * - @return - `0` on success. `-EINVAL` if `id` is not valid.
 */
int chanmgr_delete(chan_id_t id);

/***/

/*
 * TODO: Integrate event management into this or adjacent APIs.
 */

#endif /* CHANMGR_CREATE_H */
