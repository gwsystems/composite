#pragma once

/***
 * The core IPC fastpaths of the system. This includes the thread
 * migration operations of
 *
 * - thread-migration-based synchronous `invocation` of a server
 *   function, and the
 * - corresponding `return` to calling component.
 *
 * It also includes the IPC fastpaths for synchronous RPC between
 * rendezvousing threads. This is similar to L4-style IPC and includes
 * the typical functions:
 *
 * - `call` which both blocks the current thread, and awaits
 *   reactivation by the server thread, and
 * - `reply_and_wait` which a server thread uses to reply to the
 *   previous thread, and await a new `call`.
 *
 * Composite also supports asynchronous activation of threads, but
 * that requires more machinery (TCaps, scheduler involvement,
 * etc...), so you can find those operations along-side the dispatch
 * path in the `thread` files.
 */

#include <compiler.h>
#include <cos_error.h>
#include <cos_types.h>
#include <thread.h>
#include <captbl.h>
#include <capabilities.h>
#include <state.h>
#include <component.h>

COS_FASTPATH static inline struct regs *
sinv_invoke(struct thread *t, uword_t *head, struct regs *rs, struct capability_sync_inv *cap)
{
	struct invstk_entry *client, *server;
	uword_t ip, sp;
	uword_t client_off = *head;
	cos_retval_t r;

	/* Avoid overflowing the thread's invocation stack */
	if (unlikely(client_off == COS_INVSTK_SIZE - 1)) COS_THROW(r, -COS_ERR_OUT_OF_BOUNDS, err);
	/* Make sure that the component we're invoking is alive */
	COS_CHECK_THROW(component_activate(&cap->intern.component), r, err);

	/* Save the client's state */
	client = &t->invstk.entries[client_off];
	regs_ip_sp(rs, &ip, &sp);
	client->ip = ip;
	client->sp = sp;

	server = &t->invstk.entries[client_off + 1];
	resource_compref_copy(&server->component, &cap->intern.component);
	(*head)++;

	/* Prepare to upcall into the server in the current registers */
	regs_prepare_upcall(rs, cap->intern.entry_ip, coreid(), t->id, cap->intern.token);

	return rs;
err:
	/* Error: leave registers the same except for a return value... */
	regs_retval(rs, REGS_RETVAL_BASE, r);

	return rs;
}

COS_FASTPATH static inline struct regs *
sinv_return(struct thread *t, uword_t *head, struct regs *rs)
{
	struct invstk_entry *client;
	uword_t ip, sp;
	cos_retval_t r;

	if (unlikely(*head == 0)) COS_THROW(r, -COS_ERR_OUT_OF_BOUNDS, err);

	/* Find the client we're returning to, check it is alive */
	client = &t->invstk.entries[*head - 1];
	COS_CHECK_THROW(component_activate(&client->component), r, err);
	(*head)--;

	/* Restore the ip/sp in the client, pass arguments implicitly, and active! */
        regs_set_ip_sp(rs, client->ip, client->sp);

	return rs;
err:
	/* Error: just add the return value. */
	regs_retval(rs, REGS_RETVAL_BASE, r);

        return rs;
}
