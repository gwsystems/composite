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
#include "../../../kernel/include/clist.h"

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
	struct clist tcap_root_list;
	struct thread *timer;
};

u32_t cyc_per_tick = CPU_GHZ * 10000000;

/* Yes.  This just happened. */
#include "../../../kernel/tcap.c"
/* Deal with it. */

void
__print_tcap(struct tcap *t)
{
	int i;

	fprintf(stderr, "tcap (%p, %d): [", 
		tcap_sched_info(t)->sched,
		tcap_sched_info(t)->prio);
	for (i = 0 ; i < t->ndelegs ; i++) {
		fprintf(stderr, "(%p, %d)%s",
			t->delegations[i].sched, 
			t->delegations[i].prio,
			i == (t->ndelegs-1) ? "" : ", ");
	}
	fprintf(stderr, "]\n");
}

#define CYC_PLACEHOLDER (1024LL)
#define CYC_MIN (1LL)
typedef enum {PRIO_HI = 1, PRIO_MED, PRIO_LO} prio_t;

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

/* max delegations for now */
#define NDELEGS 6
#define NCDELEG 2

struct comps {
	struct spd c;
	struct tcap *r, *ts[NCDELEG];
	int prios[NCDELEG], chosen;
};

struct comps_summary {
	struct priorities { int ps[NCDELEG];} prios[NDELEGS];
	int order[NDELEGS];
	int n;
};

extern int
__tcap_higher_prio(struct tcap *a, struct tcap *c);

int
__higher_prio(struct comps *cs, int *order, int off)
{
	int i;

	for (i = 0 ; i < off ; i++) {
		int o = order[i];

		if (cs[o].prios[1] > cs[o].prios[0]) return 0;
	}
	return 1;
}

int
__transfer_is_legal(struct comps *cs, int *order, int n, 
		    int off, int expected)
{
	int i;


	for (i = 1 ; i < off ; i++) {
		int o = order[i];
		if (cs[o].prios[0] > cs[o].prios[1]) {
			if (!expected) {
				fprintf(stderr, "transfer is legal: expected not to transfer @ %d (up to %d)\n", i, off);
				for (i = 1 ; i < n ; i++) {
					int o = order[i];
					fprintf(stderr, "(%d >? %d), ", 
					       cs[o].prios[0], cs[o].prios[1]);
				}
				fprintf(stderr, "\n");
				__print_tcap(cs[order[off]].ts[0]);
				__print_tcap(cs[order[off]].ts[1]);
			}
			return 1;
		}
	}
	if (expected) {
		fprintf(stderr, "transfer is not legal: expected to transfer @ %d (up to %d)\n", i, off);
		for (i = 1 ; i < n ; i++) {
			int o = order[i];
			fprintf(stderr, "(%d >? %d), ", 
			       cs[o].prios[0], cs[o].prios[1]);
		}
		fprintf(stderr, "\n");
		__print_tcap(cs[order[off]].ts[0]);
		__print_tcap(cs[order[off]].ts[1]);
	}
	return 0;
}

void
__gen_rand(struct comps *cs, int *order, int n)
{
	int i, j, k, ninit = n;

	memset(cs, 0, sizeof(struct comps) * n);
	memset(order, 0, sizeof(int) * n);

	for (i = 0 ; i < n ; i++, ninit--) {
		int rv, nchosen = 0;

		rv = rand() % ninit;
		for (j = 0 ; 1 ; j++) {
			if (cs[j].chosen) continue;
			if (nchosen == rv) break;
			nchosen++;
		}
		assert(!cs[j].chosen);
		cs[j].chosen = 1;
		order[i]     = j;
		for (k = 0 ; k < NCDELEG ; k++) {
			cs[j].prios[k] = rand() % TCAP_PRIO_MIN;	
		}
	}
}

void
__gen_specific(struct comps_summary *s, struct comps *cs, int *order, int n)
{
	int i, j;

	memset(cs, 0, sizeof(struct comps) * n);
	memset(order, 0, sizeof(int) * n);
	memcpy(order, s->order, n * sizeof(int));

	for (i = 0 ; i < n ; i++) {
		int o = order[i];

		for (j = 0 ; j < NCDELEG ; j++) {
			cs[o].prios[j] = s->prios[o].ps[j];
		}
		cs[o].chosen = 1;
	}
}

void 
__print_specific(struct comps *cs, int *order, int n, int line)
{
	int i, j;

	fprintf(stderr, "Error case @ %d:\n", line);
	fprintf(stdout, "{ .prios = {\n");
	for (i = 0 ; i < n ; i++) {
		fprintf(stdout, "\t{.ps = {");
		for (j = 0 ; j < NCDELEG ; j++) {
			fprintf(stdout, "%d%s", cs[i].prios[j], 
			       j == (NCDELEG-1) ? "" : ", ");
		}
		fprintf(stdout, "}}%s\n", i == (n-1) ? "" : ",");
	}

	fprintf(stdout, "   },\n  .order = {");
	for (i = 0 ; i < n ; i++) {
		fprintf(stdout, "%d%s", order[i], i == (n-1) ? "},\n" : ", ");
	}
	fprintf(stdout, "  .n = %d\n},\n", n);
}

#define tassert(p) do { int t = (int)(void*)p; if (!(t)) { __print_specific(cs, order, n, __LINE__); assert((t)); }} while(0)

void
__test_delegations(struct comps *cs, int *order, int n)
{
	struct spd *r;
	int i, j;

	r = &cs[order[0]].c;
	tcap_spd_init(r);
	tcap_root(r);
	tassert(cs[order[0]].chosen);
	cs[order[0]].ts[0] = tcap_split(tcap_get(r, 0), 0, 0);
	cs[order[0]].ts[1] = tcap_split(tcap_get(r, 0), 0, 0);
	for (i = 1 ; i < n ; i++) {
		int o = order[i], prev = order[i-1];
		
		tassert(cs[o].chosen);
		tcap_spd_init(&cs[o].c);
		tcap_root_alloc(&cs[o].c, tcap_get(r, 0), 1, 0);
		cs[o].r = tcap_get(&cs[o].c, 0);
		for (j = 0 ; j < NCDELEG ; j++) {
			int p;

			cs[o].ts[j] = tcap_split(cs[o].r, 0, 0);
			tassert(cs[o].ts[j]);

			p = cs[prev].prios[j];
			tassert(!tcap_delegate(cs[o].ts[j], cs[prev].ts[j], 0, p));
			tassert(cs[o].ts[j]);
//			cs[prev].prios[j] = p;
//			cs[o].prios[j] = 0;
		}
	}

	/* for (i = 0 ; i < n ; i++) {	 */
	/* 	int o = order[i]; */
		
	/* 	printf("(%d, %d)\n", cs[o].prios[0], cs[o].prios[1]); */
	/* } */

	/* now the tests!...transfer: */
	for (i = 1 ; i < n ; i++) {
		int o = order[i];
		
		if (tcap_transfer(cs[o].ts[1], cs[o].ts[0], 0, 0)) {
			tassert(__transfer_is_legal(cs, order, n, i, 1));
		} else {
			tassert(!__transfer_is_legal(cs, order, n, i, 0));
		}
	}
	/* ...and higher priority... */
	for (i = 1 ; i < n ; i++) {
		int o = order[i];
		
		if (__tcap_higher_prio(cs[o].ts[1], cs[o].ts[0])) {
			tassert(__higher_prio(cs, order, i));
		} else {
			tassert(!__higher_prio(cs, order, i));
		}
	}

	for (i = 0 ; i < n ; i++) {
		tcap_spd_delete(&cs[i].c);
	}
}

#define ITER 1000

void
test_fuzz_delegations(void)
{
	struct comps cs[NDELEGS];
	int order[NDELEGS], i;
	struct comps_summary tests[] = {
#include "tcap_testcases.h"		
		{ .n = 0 }
	};

	for (i = 0 ; tests[i].n ; i++) {
		__gen_specific(&tests[i], cs, order, tests[i].n);
		__test_delegations(cs, order, tests[i].n);
	}
	fprintf(stderr, "Fuzz testing:\n");
	for (i = 0 ; i < ITER ; i++) {
		__gen_rand(cs, order, NDELEGS);
		__test_delegations(cs, order, NDELEGS);
	}
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
	tcap_root(&p);
	assert(tp);
	/* split and transfer */
	tpl = tcap_split(tp, CYC_PLACEHOLDER, PRIO_LO);
	assert(tpl);
	assert(tcap_remaining(tpl) == CYC_PLACEHOLDER);
	tpm = tcap_split(tp, CYC_PLACEHOLDER, PRIO_MED);
	assert(tpm);
	assert(tcap_remaining(tpm) == CYC_PLACEHOLDER);
	assert(!tcap_transfer(tpl, tpm, CYC_PLACEHOLDER, PRIO_LO));
	assert(tcap_remaining(tpm) == 0);
	assert(tcap_remaining(tpl) == CYC_PLACEHOLDER*2);

	/* bi-directional transfer */
	assert(!tcap_transfer(tpm, tpl, CYC_PLACEHOLDER, PRIO_MED));
	assert(!tcap_transfer(tpl, tpm, CYC_PLACEHOLDER, PRIO_LO));
	assert(tcap_remaining(tpm) == 0);
	assert(tcap_remaining(tpl) == CYC_PLACEHOLDER*2);

	assert(!tcap_merge(tp, tpl));
	tpl = tcap_split(tp, 0, PRIO_LO);
	/* split and transfer with shared budget */
	tph = tcap_split(tpl, 0, PRIO_HI);
	assert(tph);
	assert(tcap_remaining(tph) == TCAP_RES_INF);
	assert(!tcap_transfer(tpm, tph, CYC_PLACEHOLDER, PRIO_MED));
	assert(tcap_remaining(tpm) == CYC_PLACEHOLDER);
	assert(tcap_remaining(tpl) == TCAP_RES_INF);
	assert(tcap_remaining(tph) == TCAP_RES_INF);

	/* transfer between tcaps with shared budget? */
	assert(!tcap_transfer(tpl, tph, 0, PRIO_LO));
	assert(tcap_remaining(tpl) == tcap_remaining(tph));

	/* mirror operations */
	assert(tcap_get(&p, tcap_id(tp)) == tp);
	assert(tcap_get(&p, tcap_id(tpl)) == tpl);
	assert(tcap_get(&p, tcap_id(tpm)) == tpm);
	assert(tcap_get(&p, tcap_id(tph)) == tph);

	/* merge between separate budgets */
	cyc = tcap_remaining(tpm);
	assert(!tcap_merge(tpl, tpm));
	tpm = tcap_split(tpl, cyc, PRIO_MED);
	assert(tpm);
	assert(cyc == tcap_remaining((tpm)));

	/* ...and between a shared budget */
	cyc = tcap_remaining(tph);
	assert(!tcap_merge(tpl, tph));
	tph = tcap_split(tpl, cyc, PRIO_HI);
	assert(tph);
	assert(cyc == tcap_remaining(tph));
	assert(cyc == TCAP_RES_INF);
	assert(TCAP_RES_INF == tcap_remaining((tpl)));

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
	tcap_root(&c);
	tc  = tcap_get(&c, 0);
	assert(tc);
	assert(tcap_get(&c, tcap_id(tc)) == tc);
	tcl = tcap_split(tc, 0, PRIO_LO);
	tcm = tcap_split(tc, 0, PRIO_MED);
	tch = tcap_split(tc, 0, PRIO_HI);
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
	assert(tcm && tph && tpl);
	assert(!tcap_delegate(tcm, tph, CYC_MIN, 0));
	//delegations_print(tcm);

	//delegations_print(tcm);
	//delegations_print(tpm);
	assert(!tcap_delegate(tcm, tpl, CYC_MIN, 0));
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
	assert(tcl && tpl);
	assert(!tcap_delegate(tcl, tpl, CYC_MIN, 0));
	//delegations_print(tcl);

	assert(!tcap_higher_prio(&lo, &med));
	//delegations_print(tcm);
	//delegations_print(tcl);
	assert(tcap_higher_prio(&med, &lo));
	
	tgc = tcap_get(&gc, 0);
	assert(tgc);
	assert(tcap_get(&gc, tcap_id(tgc)) == tgc);
	tgcl = tcap_split(tc, 0, PRIO_LO);
	tgcm = tcap_split(tc, 0, PRIO_MED);
	tgch = tcap_split(tc, 0, PRIO_HI);
	assert(tgcl && tgcm && tgch);
	assert(tgc && tpl);
	assert(!tcap_delegate(tgc, tpl, CYC_PLACEHOLDER/2, PRIO_MED));

	/* multi-sched delegations */
#define split(n)						\
	t##n##a = tcap_split(tcap_get(&s##n, 0), CYC_PLACEHOLDER, PRIO_LO); \
	assert(t##n##a);						\
	t##n##b = tcap_split(tcap_get(&s##n, 0), CYC_PLACEHOLDER, PRIO_HI); \
	assert(t##n##b);

	tcap_spd_init(&s0);
	tcap_spd_init(&s1);
	tcap_spd_init(&s2);
	tcap_spd_init(&s3);

	tcap_delegate(tcap_get(&s0, 0), tp, 0, PRIO_MED);
	tcap_delegate(tcap_get(&s1, 0), tp, 0, PRIO_MED);
	tcap_delegate(tcap_get(&s2, 0), tp, 0, PRIO_MED);
	tcap_delegate(tcap_get(&s3, 0), tp, 0, PRIO_MED);
	
	split(0);
	split(1);
	split(2);
	split(3);

	t0a = tcap_split(tcap_get(&s0, 0), CYC_PLACEHOLDER, PRIO_MED);
	assert(t2a && t3a);
	assert(!tcap_delegate(t2a, t3a, 1, 0));
	assert(!tcap_delegate(t3b, t2b, 1, 0));
	assert(!tcap_delegate(t3b, t2b, 1, 0));
	assert(!tcap_delegate(t0a, t1a, 1, 0));
	assert(!tcap_delegate(t1a, t2a, 1, 0));
	assert(!tcap_delegate(t2b, t1b, 1, 0));

	assert(!tcap_delegate(t1a, t3b, 1, 0));
	assert(!tcap_delegate(t2a, t0a, 1, 0));
	assert(!tcap_delegate(t1a, t2a, 1, 0));

	tcap_spd_delete(&p);
	tcap_spd_delete(&gc);
	tcap_spd_delete(&c);
	tcap_spd_delete(&s0);
	tcap_spd_delete(&s1);
	tcap_spd_delete(&s2);
	tcap_spd_delete(&s3);
}

int
main(void)
{
	unsigned long long tsc;

	rdtscll(tsc);
	srand((unsigned int)tsc);
	test_unit_manual();
	test_fuzz_delegations();

	return 0;
}
