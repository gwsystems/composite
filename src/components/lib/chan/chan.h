/*
 * Copyright 2020, Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#ifndef CHAN_H
#define CHAN_H

/***
 * Channel implementation that enables intra- and inter-core
 * communication. Currently, we assume only single-producer,
 * single-consumer communication (SPSC).
 */

/* Internal implementation details of the channel */
#include <chan_private.h>
#include <evt.h>
#include <chan_types.h>

/**
 * `chan_send` sends an item to a channel. The size of the item is
 * provided by the channel creation APIs. The flags indicate if we're
 * just trying to peak (and not remove the time), or if we want to
 * make this call nonblocking. Note: this is inlined to avoid the
 * branch overheads as the compiler inlines the flags.
 *
 * - @c      - Channel to receive from.
 * - @item   - The item to copy into the channel.
 * - @flags  - The flags.
 * - @return - One of these values:
 *
 *     - `0` on success
 *     - `CHAN_TRY_AGAIN` if `CHAN_NONBLOCKING` was passed in, and
 *       the channel is full.
 *     - `-CHAN_ERR_*` if an error occurred.
 */
static inline int
chan_send(struct chan_snd *c, void *item, chan_comm_t flags)
{
	int ret;

	ret = __chan_send_pow2(c, item, c->meta.wraparound_mask, c->meta.item_sz, !(flags & CHAN_NONBLOCKING));
	if (likely(ret == 0)) {
		return 0;
	} else if (ret > 0) {
		assert(flags & CHAN_NONBLOCKING);
		return CHAN_TRY_AGAIN;
	} else { 		/* negative value = error */
		return -CHAN_ERR_INVAL_ARG;
	}
}

/**
 * `chan_recv` reads an item off of the channel. The size of the item
 * is provided by the channel creation APIs. The flags indicate if
 * we're just trying to peak (and not remove the time), or if we want
 * to make this call nonblocking. Note: this is inlined to avoid the
 * branch overheads as the compiler inlines the flags.
 *
 * - @c      - Channel to receive from.
 * - @item   - The memory to copy the item into.
 * - @flags  - The flags.
 * - @return - One of these values:
 *
 *     - `0` on success
 *     - `CHAN_TRY_AGAIN` if `CHAN_NONBLOCKING` was passed in, and
 *       there is not data in the channel to receive.
 *     - `-CHAN_ERR_*` if an error occurred.
 */
static inline int
chan_recv(struct chan_rcv *c, void *item, chan_comm_t flags)
{
	int ret;

	ret = __chan_recv_pow2(c, item, c->meta.wraparound_mask, c->meta.item_sz, !(flags & CHAN_NONBLOCKING));
	if (likely(ret == 0)) {
		return 0;
	} else if (ret > 0) {
		assert(flags & CHAN_NONBLOCKING);
		return CHAN_TRY_AGAIN;
	} else { 		/* negative value = error */
		return -CHAN_ERR_INVAL_ARG;
	}
}

/**
 * `chan_init` initializes a channel data-structure, and creates a new
 * channel with `slots` items each of maximum size `item_sz`.
 *
 * - @item_sz - The number of bytes in each item.
 * - @nslots  - The number of items the channel can buffer.
 * - @flags   - requested invariants and usage patterns on the channel
 * - @return  - `0` on success, `-errval` where `errval` is one of the above `CHAN_ERR_*` values.
 */
int chan_init(struct chan *c, unsigned int item_sz, unsigned int nslots, chan_flags_t flags);

/**
 * `chan_snd|rcv_init` initializes a new sender or receiver endpoint.
 *
 * - @ep     - The send or receive endpoint to populate.
 * - @c      - The channel for which we're creating the end-point.
 * - @return - `0` on success, `-errval`  in `CHAN_ERR_*`.
 */
int chan_snd_init(struct chan_snd *s, struct chan *c);
int chan_rcv_init(struct chan_rcv *r, struct chan *c);

/**
 * `chan_snd|rcv_init_with` initializes a channel associated with a
 * capability that we have been *initialized* with (thus is already in our `chanmgr` capability table).
 *
 * - @ep      - The communication endpoint structure to be initialized
 * - @cap_id  - This is a capability id that should denote the ability
 *   to access and create an end-point of the channel.
 * - @nslots  - The number of items the channel can buffer.
 * - @item_sz - The number of bytes in each item.
 * - @flags   - requested invariants and usage patterns on the channel
 * - @return  - The channel or NULL on error.
 */
int chan_snd_init_with(struct chan_snd *s, chan_id_t cap_id, unsigned int item_sz, unsigned int nslots, chan_flags_t flags);
int chan_rcv_init_with(struct chan_rcv *r, chan_id_t cap_id, unsigned int item_sz, unsigned int nslots, chan_flags_t flags);

/**
 * `chan_(snd|rcv_)*teardown` tears down a send/recv endpoint, and/or
 * dereferences a corresponding channel. If it is the last reference,
 * it will teardown and deallocate the different aspects of the
 * channel. The return value indicates if the `struct chan` is no
 * longer active. If the channel, was initialized with
 * `CHAN_DEALLOCATE`, the endpoint or the channel will be `free`d. For
 * `chan_teardown`, the return value must be used to determine if the
 * channel was actually torn down, or if references exist to it.
 *
 * - @ep     - the reference to the channel communication endpoint.
 * - @return - `0` if `c` is no longer referenced, and it has been
 *   torn down, `1` if it is still referenced. In such a case,
 *   `chan_teardown` should be called again later until it succeeds.
 */
void chan_snd_teardown(struct chan_snd *s);
void chan_rcv_teardown(struct chan_rcv *r);
int  chan_teardown(struct chan *c);

/**
 * How much memory is required for the channel?
 *
 * - @item_sz - size of each item
 * - @slots   - number of items
 * - @return  - number of bytes required for the channel's memory
 */
unsigned int chan_mem_sz(unsigned int item_sz, unsigned int slots);

/**
 * Add the event resource id into the channel so that when a send
 * occurs, it will trigger the event of the receiver. This should only
 * be called on the receiver side as we only currently support adding
 * receivers into an event set. `disassociate` removes the previous
 * association.
 * All associations are for the receiver endpoint, even if the sender
 * did this with chan_snd_evt_associate (in which case it helps the
 * receiver to set up the association).
 *
 * - @c      - channel to add to the event set
 * - @eid    - the event resource id to be triggered
 * - @return -
 *
 *     - `0` on success
 *     - `-n` for error with `n` being an errno value
 */
int chan_evt_associate(struct chan *c, evt_res_id_t eid);
int chan_rcv_evt_associate(struct chan_rcv *r, evt_res_id_t eid);
int chan_snd_evt_associate(struct chan_snd *s, evt_res_id_t eid);

evt_res_id_t chan_evt_associated(struct chan *c);
evt_res_id_t chan_rcv_evt_associated(struct chan_rcv *r);
evt_res_id_t chan_snd_evt_associated(struct chan_snd *s);

int chan_evt_disassociate(struct chan *c);
int chan_rcv_evt_disassociate(struct chan_rcv *r);
int chan_snd_evt_disassociate(struct chan_snd *s);

/**
 * The following are the APIs for dynamic memory allocation of
 * channels. This is only compiled into your component if they are
 * used. Each of these functions calls the underlying `_init_` APIs,
 * and channels are initialized with `CHAN_DEALLOCATE`. You should not
 * mix the `alloc` APIs with the `init` APIs as `CHAN_DEALLOCATE`
 * applies to all channel resources, yet it is passed only to the
 * channel creation.
 */

/**
 * `chan_alloc` allocates a new channel. Calls `chan_init` internally.
 *
 * - @item_sz - The number of bytes in each item.
 * - @nslots  - The number of items the channel can buffer.
 * - @flags   - requested invariants and usage patterns on the channel
 * - @return  - The channel or `NULL` on error.
 */
struct chan *chan_alloc(unsigned int item_sz, unsigned int slots, chan_flags_t flags);

/**
 * `chan_snd|rcv_alloc` creates a new sender or receiver
 * endpoint. Calls `chan_snd|rcv_init` internally.
 *
 * - @c      - The channel for which we're creating the end-point.
 * - @return - return the new endpoint or `NULL` on error.
 */
struct chan_snd *chan_snd_alloc(struct chan *c);
struct chan_rcv *chan_rcv_alloc(struct chan *c);

/**
 * `chan_snd|rcv_get_chan` returns the channel associated with the
 * endpoint.
 *
 * These APIs are typically used to handle the garbage collection of
 * dependent resources. If a `chan` is released, but is not
 * immediately available for reclamation, you should check if it can
 * be reclaimed after releasing each `snd` and `rcv` endpoint. This
 * means getting the channel pointer to check, which is the main
 * purpose of this API.
 *
 * - @endpoint - The endpoint created from the channel to be returned.

 * - @return - The channel the endpoint was created from, or `NULL` if
 *             the endpoint was created with any of the `*_with` APIs.
 */
struct chan *chan_snd_get_chan(struct chan_snd *s);
struct chan *chan_rcv_get_chan(struct chan_rcv *r);

/**
 * `chan_snd|rcv_alloc_with` allocate a `chan_snd|rcv`, and call the
 * corresponding `*init*` functions.
 *
 * - @id      - the channel id a component has been initialized with.
 * - @item_sz - presumed size of each item.
 * - @nslots  - presumed number of items in the channel.
 * - @flags   - presumed flags
 * - @return  - `NULL` on failure, or the endpoint
 */
struct chan_snd *chan_snd_alloc_with(chan_id_t cap_id, unsigned int item_sz, unsigned int nslots, chan_flags_t flags);
struct chan_rcv *chan_rcv_alloc_with(chan_id_t cap_id, unsigned int item_sz, unsigned int nslots, chan_flags_t flags);

/**
 * Free the corresponding channel resources. These all assume that
 * when the channel was created, it was created with the `_alloc_`
 * APIs. Note that the channel deallocation might be delayed due to
 * reference-counted garbage collection.
 *
 * - @chan - The channel to deallocate.
 */
void chan_free(struct chan *c);
void chan_snd_free(struct chan_snd *s);
void chan_rcv_free(struct chan_rcv *r);

#endif /* CHAN_H */
