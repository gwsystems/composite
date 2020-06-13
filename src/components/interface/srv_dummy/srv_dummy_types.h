/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#ifndef SRV_DUMMY_TYPES_H
#define SRV_DUMMY_TYPES_H

#include <cos_types.h>
#include <srv_dummy.h>

typedef enum {
	SRV_DUMMY_HELLO = 0,
	SRV_DUMMY_GOODBYE = 1,
} srv_dummy_api_t;

#define SRV_DUMMY_MAX 2

static inline vaddr_t
srv_dummy_api(srv_dummy_api_t t)
{
	vaddr_t api = 0;

	switch(t) {
	case SRV_DUMMY_HELLO:
		api = (vaddr_t)&srv_dummy_hello;
		break;
	case SRV_DUMMY_GOODBYE:
		api = (vaddr_t)&srv_dummy_goodbye;
		break;
	default: assert(0);
	}

	return api;
}

/* TODO: keys through runscripts!! */
#define SRV_DUMMY_KEY 'A'
#define SRV_DUMMY_INSTANCE(i) (SRV_DUMMY_KEY | (i << 10))

#define SRV_DUMMY_SKEY(i, t) (SRV_DUMMY_INSTANCE(i) + t)
#define SRV_DUMMY_RKEY(i, t) ((1 << 9) | (SRV_DUMMY_INSTANCE(i) + t))

#define SRV_DUMMY_ISTATIC 0
#define SRV_DUMMY_IMAX    1

#endif /* SRV_DUMMY_TYPES_H */
