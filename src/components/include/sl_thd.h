/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2017, The George Washington University
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */

#ifndef SL_THD_H
#define SL_THD_H

typedef enum {
	SL_THD_FREE = 0,
	SL_THD_BLOCKED,
	SL_THD_BLOCKED_TIMEOUT,
	SL_THD_WOKEN, 		/* if a race causes a wakeup before the inevitable block */
	SL_THD_RUNNABLE,
	SL_THD_DYING,
} sl_thd_state;

typedef enum {
	SL_THD_THD = 0,
	SL_THD_AEP,
	SL_THD_AEP_TCAP,
	SL_THD_COMP,
	SL_THD_COMP_TCAP,
} sl_thd_type;

struct sl_thd {
	sl_thd_state        state;
	sl_thd_type         type;
	thdid_t             thdid;
	struct cos_aep_info aepinfo;
	asndcap_t           sndcap;
	tcap_prio_t         prio;
	struct sl_thd      *dependency;

	tcap_res_t          budget;        /* budget if this thread has it's own tcap */
	cycles_t            last_replenish;
	cycles_t            period;
	cycles_t            periodic_cycs; /* for implicit periodic timeouts */
	cycles_t            timeout_cycs;  /* next timeout - used in timeout API */
	cycles_t            wakeup_cycs;   /* actual last wakeup - used in timeout API for jitter information, etc */
	int                 timeout_idx;   /* timeout heap index, used in timeout API */
};

static inline struct cos_aep_info *
sl_thd_aepinfo(struct sl_thd *t)
{ return &(t->aepinfo); }

static inline thdcap_t
sl_thd_thdcap(struct sl_thd *t)
{ return sl_thd_aepinfo(t)->thd; }

static inline tcap_t
sl_thd_tcap(struct sl_thd *t)
{ return sl_thd_aepinfo(t)->tc; }

static inline arcvcap_t
sl_thd_rcvcap(struct sl_thd *t)
{ return sl_thd_aepinfo(t)->rcv; }

static inline asndcap_t
sl_thd_asndcap(struct sl_thd *t)
{ return t->sndcap; }

#ifndef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0; } } while (0)
#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);
#define SPIN() do { while (1) ; } while (0)
#endif

#endif /* SL_THD_H */
