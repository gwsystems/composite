#ifndef AUTHMGR_H
#define AUTHMGR_H

#include <cos_component.h>

/***
 * This API is there to provide the authorization to use abstract
 * resources (that don't map perfectly to kernel resources) including
 * capability-like delegation and revocation.
 */

typedef word_t res_id_t;
typedef word_t authmgr_cap_t;

/**
 * Set of functions to share a higher-level resource (provided by the
 * implementer of this API, for now), that they want to be able to
 * share (delegate). There are two sets of APIs here:
 *
 * 1. those where the client wishes to delegate a resource it has
 *    created to the server, and
 * 2. those where the server wishes to delegate to the client.
 *
 * Additionally, we provide a revoke function the authmgr capability,
 * which is likely not very useful and will need to change in the
 * future.
 */
authmgr_cap_t authmgr_client_delegate(res_id_t id);
res_id_t      authmgr_server_receive(authmgr_cap_t id, compid_t client);

res_id_t      authmgr_client_receive(authmgr_cap_t id);
authmgr_cap_t authmgr_server_delegate(res_id_t id, compid_t client);

/* TODO: is authmgr_cap_t or (res_id_t, compid_t) better? */
int authmgr_revoke(authmgr_cap_t id);

#endif /* AUTHMGR_H */
