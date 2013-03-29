/**
 * Copyright (c) 2013 by The George Washington University.
 * 
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 * 
 * Initial Author: Jakob Kaivo, jkaivo@gwu.edu, 2013.
 */

#ifndef TCAP_H
#define TCAP_H 1

#include "shared/cos_types.h"

#ifndef MAX_DELEGATIONS
#define MAX_DELEGATIONS 32
#endif

struct budget {
        s32_t cycles;
};

struct tcap {
	struct budget *budget;
	struct delegation {
		u16_t priority;
		struct spd *sched;
	} delegations[MAX_DELEGATIONS];
	u16_t ndels;
};

struct tcap *tcap_delegate (struct spd *comp, struct tcap *tcap);

struct tcap *tcap_activate (struct tcap *tcap);

struct tcap *tcap_transfer (struct tcap *tcapdst, struct tcap *tcapsrc, struct budget *budget);

struct tcap *tcap_revoke (struct spd *comp, struct tcap *tcap);


#endif
