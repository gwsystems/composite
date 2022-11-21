#ifndef SYNCIPC_H
#define SYNCIPC_H

#include <cos_component.h>

/***
 * A simple API to mimic the L4-based synchronous rendezvous between
 * threads. Use `call` and `reply_wait` to minimize "system calls" (in
 * our case, thread migration-based invocations).
 */

/**
 * `syncipc_call` invokes the IPC endpoint (`ipc_ep`), which is an
 * opaque identifier for an endpoint, passing two arguments, and
 * awaits two reply arguments. Another thread, rendezvousing on the
 * endpoint, is the communicating pair.
 */
int syncipc_call(int ipc_ep, word_t arg0, word_t arg1, word_t *ret0, word_t *ret1);

/**
 * `syncipc_reply_wait` replies to the most recent `call` on an ipc
 * endpoint with two arguments, and returns the arguments from the
 * next. If there was no previous `call` for which to return, the
 * arguments are ignored. Either way, the current thread blocks
 * awaiting a `call`.
 */
int syncipc_reply_wait(int ipc_ep, word_t arg0, word_t arg1, word_t *ret0, word_t *ret1);

#endif /* SYNCIPC_H */
