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
#define MAX_DELEGATIONS 8
#endif

struct budget {
        s32_t cycles;
};

struct tcap {
	/* 
	 * The budget might be from a shared pool in which case budget
	 * refers to the parent tcap, or it might be segregated in
	 * this capability in which case budget = this.
	 */
	struct tcap  *budget;
	struct budget budget_local;
	u16_t         ndels, priority;
	unsigned int  epoch, cpuid;

	/* 
	 * Which chain of temporal capabilities resulted in this
	 * capability's access, and what access is granted? The tcap
	 * is sched->tcaps[tcap_off].
	 */
	struct delegation {
		struct spd *sched;
 		u16_t       priority, tcap_off;
	} delegations[MAX_DELEGATIONS];
};

/* 
 * Active temporal capability in the thread structure.  This is used
 * to transfer budget, and maintain a record for which tcap is
 * actually active for a thread.
 */
struct tcap_active {
	/* 
	 * cap_active is the active tcap (that is charged for
	 * execution), and cap_sink is provided by a scheduler that is
	 * being delegated to, and when the "parent" delegates to a
	 * thread with this sink, budget/priority will be transferred
	 * over.
	 */
	struct tcap *cap_active;
	struct spd  *cap_delegater;
	u16_t        cap_del_off;
};

struct tcap *tcap_delegate(struct spd *comp, struct tcap *tcap);
struct tcap *tcap_activate(struct tcap *tcap);
struct tcap *tcap_transfer(struct tcap *tcapdst, struct tcap *tcapsrc, struct budget *budget);
struct tcap *tcap_revoke(struct spd *comp, struct tcap *tcap);


#endif
