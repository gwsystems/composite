#ifndef CHAN_TYPES_H
#define CHAN_TYPES_H

/***
 * Channel API return values. `int` return values are `0` on success,
 * one of these values, or if an error, a negative value of one of
 * these.
 */

/*
 * Non-blocking operation couldn't complete (send found a full
 * channel, or recv found an empty channel)
 */
#include <errno.h>
#define CHAN_TRY_AGAIN       EAGAIN
/* trying to deallocate an active channel */
#define CHAN_ERR_ACTIVE      EAGAIN
/* writing or reading from a channel closed on the other end */
#define CHAN_ERR_NOT_ACTIVE  EPIPE
/* cannot allocate due to a lack of memory */
#define CHAN_ERR_NOMEM       ENOMEM
/* passed an invalid argument (e..g., chan_id_t). This is used by the `_alloc_` APIs */
#define CHAN_ERR_INVAL_ARG   EINVAL

/* values used when using the communication APIs */
typedef enum {
	CHAN_NONBLOCKING = 1,
	CHAN_PEEK        = 1 << 1,
} chan_comm_t;

/* `chan_id_t`s with value `0` signal error */
typedef word_t chan_id_t;

/* Values for channel initialization */
typedef enum {
	CHAN_DEFAULT    = 0,
	CHAN_MPSC       = 1,	  /* !CHAN_MPSC == SPSC */
	CHAN_EXACT_SIZE = 1 << 1, /* The channel size cannot be higher than its initialization size */
	CHAN_DEALLOCATE = 1 << 2  /* used internally for the `_alloc` APIs */
} chan_flags_t;

#endif	/* CHAN_TYPES_H */
