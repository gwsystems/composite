#include "tcap.h"

struct tcap *
tcap_delegate (struct spd *comp, struct tcap *tcap)
{
	assert (tcap);
	if (tcap->ndels >= MAX_DELEGATIONS) {
		printk ("tcap %x already has max number of delgations.\n", tcap);
		return NULL;
	}
	tcap->delegations[tcap->ndels].priority = DEFAULT_PRIORITY;
	tcap->delegations[tcap->ndels].sched = comp;
	tcap->ndels++;
	return tcap;
}

struct tcap *
tcap_activate (struct tcap *tcap)
{
	// set the active tcap (in what? the current thread? current spd?)
	return tcap;
}

struct tcap *
tcap_transfer (struct tcap *tcapdst, struct tcap *tcapsrc, struct budget *budget)
{
	assert (tcapdst);
	assert (tcapsrc);
	assert (budget);
	// check for tcapsrc->budget >= budget, otherwise indicate error or only transfer what is there (which is right?)
	tcapsrc->budget.cycles -= budget->cycles;
	tcapdst->budget.cycles += budget->cycles;
	return tcapdst;
}

struct tcap *
tcap_revoke (struct spd *comp, struct tcap *tcap)
{
	assert (tcap);
	assert (comp);
	int i;
	for (i = 0; i < tcap->ndels; i++) {
		if (tcap->delegations[i].sched == comp) {
			// remove this delegation
			// shift everything left? that's not cool
			// move the last delegation here, then ncap--? could work
		}
	}
	return tcap;
}

