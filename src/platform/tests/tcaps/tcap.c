/**
 * Unit, edge, and fuzz testing for tcaps.  The implementation of
 * simple logic is complicated enough now that I don't want to test it
 * in the kernel, thus this atrocity.
 */

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

#define unlikely(x) x
#define printk printf

/* This is only needed for compilation, and for tcap_higher_prio */
struct thread;
struct spd;

#define TCAP_MAX 32

struct sched_interaction {
	struct spd *child, *parent;
	struct thread *t;
};

#define thd_scheduled_by(t, s) 1

#include "../../../kernel/include/tcap.h"

struct thread {
	/* The currently activated tcap for this thread's execution */
	struct tcap_ref tcap_active;
	/* The tcap configured to receive delegations for this thread */
	struct tcap_ref tcap_receiver;
};

struct spd {
	unsigned int ntcaps;
	struct tcap *tcap_freelist;
	struct tcap tcaps[TCAP_MAX];
};

u32_t cyc_per_tick = CPU_GHZ * 10000000;

/* Yes.  This just happened. */
#include "../../../kernel/tcap.c"
/* Deal with it. */

#define CYC_PLACEHOLDER (1024LL)
#define CYC_MIN (1LL)
typedef enum {PRIO_HI = 1, PRIO_MED, PRIO_LO} prio_t;

void
delegations_print(struct tcap *t)
{
	int i;
	printf("tcap %p for spd %p, priority %d, %d delegations:\n", 
	       t, tcap_sched_info(t)->sched, tcap_sched_info(t)->prio, t->ndelegs);
	for (i = 0 ; i < t->ndelegs ; i++) {
		printf("\t%p, prio %d\n", 
		       t->delegations[i].sched, t->delegations[i].prio);
	}
}

int 
delegations_validate(struct tcap *t)
{
	int i;

	if (t->ndelegs-1 < t->sched_info) {
		printf("tcap %p with ndelegs %d, and current sched offset %d\n", 
		       t, t->ndelegs, t->sched_info);
		return -1;
	}
	for (i = 0 ; i < t->ndelegs ; i++) {
		if (!t->delegations[i].sched ||
		    t->delegations[i].prio > 3) {
			printf("Invalid delegation in tcap:\n");
			delegations_print(t);
			return -1;
		}
	}
	return 0;
}

#define LVLS 3

int 
_pow(int b, int e)
{
	int i;
	int v = b;

	for (i = 1 ; i < e ; i++) v *= b;

	return b;
}

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

void
test_unit_matrix(int spdoff)
{
	struct tcap *tcs[LVLS][LVLS*LVLS*LVLS];
	struct thread thds[LVLS][LVLS*LVLS*LVLS];
#define get_spd(i) (&ss[(i+spdoff)%LVLS])
	struct spd ss[LVLS];
	struct tcap *roots[LVLS];
	int i, j;
#define get_prio(i) (i%LVLS)
	unsigned long long start, end;
	unsigned long long split_tot = 0, deleg_tot = 0, highest_tot = 0, merge_tot = 0;
	int split_cnt = 0, deleg_cnt = 0, highest_cnt = 0, merge_cnt = 1;

	for (i = 0 ; i < LVLS ; i++) {
		tcap_spd_init(get_spd(i));
		roots[i] = tcap_get(get_spd(i), 0);
		assert(roots[i]);
	}

	for (i = 0 ; i < LVLS ; i++) {
		int upper = _pow(LVLS, i+1);
		rdtscll(start);
		for (j = 0 ; j < upper ; j++) {
			tcs[i][j] = tcap_split(roots[i], CYC_PLACEHOLDER, get_prio(j), 0);
			assert(tcs[i][j]);
			delegations_validate(tcs[i][j]);
		}
		rdtscll(end);
		split_tot += end-start;
		split_cnt += upper;
	}

	for (i = 1 ; i < LVLS ; i++) {
		int upper = _pow(LVLS, i+1);
		rdtscll(start);
		for (j = 0 ; j < upper ; j++) {
			//delegations_print(tcs[i-1][j/LVLS]);
			//delegations_print(tcs[i][j]);
			if (tcap_delegate(tcs[i][j], tcs[i-1][j/LVLS],
					  CYC_MIN, 0, 0)) {
				printf("Cannot delegate from [%d][%d] \n", i-1, j/LVLS);
				delegations_print(tcs[i-1][j/LVLS]);
				printf("to [%d][%d]\n", i, j);
				delegations_print(tcs[i][j]);
				assert(0);
			}
			//delegations_validate(tcs[i][j]);
		}		
		rdtscll(end);
		deleg_tot += end-start;
		deleg_cnt += upper;
	}

	for (i = 1 ; 0 && i < LVLS ; i++) {
		int upper = _pow(LVLS, i+1);
		rdtscll(start);
		for (j = 0 ; j < upper ; j++) {
			assert(!tcap_merge(tcs[i-1][j/(_pow(LVLS, i))], 
					   tcs[i][j]));
		}		
		rdtscll(end);
		merge_tot += end-start;
		merge_cnt += upper;
	}
	
	printf("Cycle costs:  split %lld, delegate %lld, merge %lld\n", 
	       split_tot/split_cnt, deleg_tot/deleg_cnt, merge_tot/merge_cnt);
}

void
test_unit_manual(void)
{
	struct spd p, gc, c;
	struct tcap *tp, *tpl, *tpm, *tph;
	struct tcap *tc, *tcl, *tcm, *tch;
	struct tcap *tgc, *tgcl, *tgcm, *tgch;
	struct thread hi, lo, med;

	struct spd s0, s1, s2, s3;
	struct tcap *t0a, *t0b, *t1a, *t2a, *t3a, *t1b, *t2b, *t3b;

	s64_t cyc;

	tcap_spd_init(&p);
	tcap_spd_init(&c);
	tcap_spd_init(&gc);

	/*
	 * Lets start off with interactions _within_ single a
	 * component.
	 */

	tp = tcap_get(&p, 0);
	assert(tp);
	/* split and transfer */
	tpl = tcap_split(tp, CYC_PLACEHOLDER, PRIO_LO, 0);
	assert(tpl);
	assert(tcap_remaining(tpl) == CYC_PLACEHOLDER);
	tpm = tcap_split(tp, CYC_PLACEHOLDER, PRIO_MED, 0);
	assert(tpm);
	assert(tcap_remaining(tpm) == CYC_PLACEHOLDER);
	assert(!tcap_transfer(tpl, tpm, CYC_PLACEHOLDER, PRIO_LO, 0));
	assert(tcap_remaining(tpm) == 0);
	assert(tcap_remaining(tpl) == CYC_PLACEHOLDER*2);

	/* bi-directional transfer */
	assert(!tcap_transfer(tpm, tpl, CYC_PLACEHOLDER, PRIO_MED, 0));
	assert(!tcap_transfer(tpl, tpm, CYC_PLACEHOLDER, PRIO_LO, 0));
	assert(tcap_remaining(tpm) == 0);
	assert(tcap_remaining(tpl) == CYC_PLACEHOLDER*2);

	/* split and transfer with shared budget */
	tph = tcap_split(tpl, 0, PRIO_HI, 1);
	assert(tph);
	assert(tcap_remaining(tph) == CYC_PLACEHOLDER*2);
	assert(!tcap_transfer(tpm, tph, CYC_PLACEHOLDER, PRIO_MED, 0));
	assert(tcap_remaining(tpm) == CYC_PLACEHOLDER);
	assert(tcap_remaining(tpl) == CYC_PLACEHOLDER);
	assert(tcap_remaining(tph) == CYC_PLACEHOLDER);

	/* transfer between tcaps with shared budget? */
	assert(!tcap_transfer(tpl, tph, 0, PRIO_LO, 0));
	assert(tcap_remaining(tpl) == tcap_remaining(tph));

	/* mirror operations */
	assert(tcap_get(&p, tcap_id(tp)) == tp);
	assert(tcap_get(&p, tcap_id(tpl)) == tpl);
	assert(tcap_get(&p, tcap_id(tpm)) == tpm);
	assert(tcap_get(&p, tcap_id(tph)) == tph);

	/* merge between separate budgets */
	cyc = tcap_remaining(tpm);
	assert(!tcap_merge(tpl, tpm));
	tpm = tcap_split(tpl, cyc, PRIO_MED, 0);
	assert(tpm);
	assert(cyc == tcap_remaining((tpm)));
	assert(CYC_PLACEHOLDER == tcap_remaining((tpl)));

	/* ...and between a shared budget */
	cyc = tcap_remaining(tph);
	assert(!tcap_merge(tpl, tph));
	tph = tcap_split(tpl, cyc, PRIO_HI, 1);
	assert(tph);
	assert(cyc == tcap_remaining((tph)));
	assert(CYC_PLACEHOLDER == tcap_remaining((tpl)));

	/* tests for trivial priority comparisons */
	assert(!tcap_bind(&lo, tpl));
	assert(!tcap_bind(&med, tpm));
	assert(!tcap_bind(&hi, tph));
	assert(tcap_higher_prio(&hi, &med));
	assert(tcap_higher_prio(&hi, &lo));
	assert(tcap_higher_prio(&med, &lo));
	assert(!tcap_higher_prio(&med, &hi));
	assert(!tcap_higher_prio(&lo, &hi));
	assert(!tcap_higher_prio(&lo, &med));
	
	/* 
	 * Now the same tests with a child and grand-child, followed
	 * by tests on transfers given a simple delegation chain.
	 */
	tc  = tcap_get(&c, 0);
	assert(tc);
	assert(tcap_get(&c, tcap_id(tc)) == tc);
	tcl = tcap_split(tc, 0, PRIO_LO, 1);
	tcm = tcap_split(tc, 0, PRIO_MED, 1);
	tch = tcap_split(tc, 0, PRIO_HI, 1);
	assert(!tcap_bind(&lo, tcl));
	assert(!tcap_bind(&med, tcm));
	assert(!tcap_bind(&hi, tch));
	assert(tcap_higher_prio(&hi, &med));
	assert(tcap_higher_prio(&hi, &lo));
	assert(tcap_higher_prio(&med, &lo));
	assert(!tcap_higher_prio(&med, &hi));
	assert(!tcap_higher_prio(&lo, &hi));
	assert(!tcap_higher_prio(&lo, &med));

	//delegations_print(tcm);
	//delegations_print(tph);
	assert(!tcap_delegate(tcm, tph, CYC_MIN, 0, 0));
	//delegations_print(tcm);

	//delegations_print(tcm);
	//delegations_print(tpm);
	assert(!tcap_delegate(tcm, tpl, CYC_MIN, 0, 0));
	//delegations_print(tcm);

	/* tcm should now have the _medium_ priority at the
	 * parent-level */
	assert(!tcap_bind(&med, tcm));
	//delegations_print(tcm);
	//delegations_print(tch);
	//delegations_print(tcl);
	assert(tcap_higher_prio(&hi, &med));
	assert(!tcap_higher_prio(&lo, &med));

	//delegations_print(tcl);
	//delegations_print(tpl);
	assert(!tcap_delegate(tcl, tpl, CYC_MIN, 0, 0));
	//delegations_print(tcl);

	assert(!tcap_higher_prio(&lo, &med));
	//delegations_print(tcm);
	//delegations_print(tcl);
	assert(tcap_higher_prio(&med, &lo));
	
	tgc = tcap_get(&gc, 0);
	assert(tgc);
	assert(tcap_get(&gc, tcap_id(tgc)) == tgc);
	tgcl = tcap_split(tc, 0, PRIO_LO, 1);
	tgcm = tcap_split(tc, 0, PRIO_MED, 1);
	tgch = tcap_split(tc, 0, PRIO_HI, 1);
	assert(tgcl && tgcm && tgch);
	assert(!tcap_delegate(tgc, tpl, CYC_PLACEHOLDER/2, PRIO_MED, 0));

	/* multi-sched delegations */
#define split(n)						\
	t##n##a = tcap_split(tcap_get(&s##n, 0), CYC_PLACEHOLDER, PRIO_LO, 0); \
	t##n##b = tcap_split(tcap_get(&s##n, 0), CYC_PLACEHOLDER, PRIO_HI, 0)

	tcap_spd_init(&s0);
	tcap_spd_init(&s1);
	tcap_spd_init(&s2);
	tcap_spd_init(&s3);

	split(0);
	split(1);
	split(2);
	split(3);

	t0a = tcap_split(tcap_get(&s0, 0), CYC_PLACEHOLDER, PRIO_MED, 0);
	assert(!tcap_delegate(t2a, t3a, 1, 0, 0));
	assert(!tcap_delegate(t3b, t2b, 1, 0, 0));
	assert(!tcap_delegate(t3b, t2b, 1, 0, 0));
	assert(!tcap_delegate(t0a, t1a, 1, 0, 0));
	assert(!tcap_delegate(t1a, t2a, 1, 0, 0));
	assert(!tcap_delegate(t2b, t1b, 1, 0, 0));

	assert(!tcap_delegate(t1a, t3b, 1, 0, 0));
	assert(!tcap_delegate(t2a, t0a, 1, 0, 0));
	assert(!tcap_delegate(t1a, t2a, 1, 0, 0));
	
	
}

int
main(void)
{
	test_unit_manual();
	test_unit_matrix(0);
	test_unit_matrix(1);
	test_unit_matrix(2);

	return 0;
}
