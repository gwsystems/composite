#pragma once

#include <cos_consts.h>
#include <cos_kern_types.h>
#include <cos_resources.h>

/*
 * A generic version of the shared structure of all capabilities. The
 * values in `type` determine what the rest of the structure looks
 * like (see the capability_* structs below). The values in
 * `operations` determine which operations can be performed on the
 * resource referenced by this capability.
 */
struct capability_generic {
	cos_cap_type_t       type;
	cos_op_bitmap_t      operations;
	liveness_t           liveness;
	uword_t              intern; /* just a placeholder we can use to get the address. */
	char                 padding[64 - (sizeof(cos_cap_type_t) + sizeof(cos_op_bitmap_t) + sizeof(liveness_t) + sizeof(uword_t))];
};

struct capability_component_intern {
	pageref_t            comp;
	epoch_t              epoch;
};

struct capability_component {
	cos_cap_type_t       type;
	cos_op_bitmap_t      operations;
	liveness_t           liveness;

	struct capability_component_intern intern;
};

struct capability_sync_inv_intern {
	inv_token_t          token;
	vaddr_t              entry_ip;
	struct component_ref component;
};

struct capability_sync_inv {
	cos_cap_type_t       type;
	cos_op_bitmap_t      operations;
	liveness_t           liveness;

	struct capability_sync_inv_intern intern;
};

struct capability_resource_intern {
	pageref_t            ref;
};

/* Threads, resource tables, and control blocks */
struct capability_resource {
	cos_cap_type_t       type;
	cos_op_bitmap_t      operations;
	liveness_t           liveness;

	struct capability_resource_intern intern;
};

struct capability_hw_intern {
	uword_t              data0;
	uword_t              data1;
	uword_t              data2;
	uword_t              data3;
	uword_t              data4;
	uword_t              data5;
};

struct capability_hw {
	cos_cap_type_t       type;
	cos_op_bitmap_t      operations;
	liveness_t           liveness;

	struct capability_hw_intern intern;
};
