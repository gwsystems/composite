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

#define CYC_PLACEHOLDER (1024LL)
#define CYC_MIN (1LL)
typedef enum {PRIO_HI = 1, PRIO_MED, PRIO_LO} prio_t;

/* void */
/* delegations_print(struct tcap *t) */
/* { */
/* 	int i; */
/* 	printf("tcap %p for spd %p, priority %d, %d delegations:\n",  */
/* 	       t, tcap_sched_info(t)->sched, tcap_sched_info(t)->prio, t->ndelegs); */
/* 	for (i = 0 ; i < t->ndelegs ; i++) { */
/* 		printf("\t%p, prio %d\n",  */
/* 		       t->delegations[i].sched, t->delegations[i].prio); */
/* 	} */
/* } */

/* int  */
/* delegations_validate(struct tcap *t) */
/* { */
/* 	int i; */

/* 	if (t->ndelegs-1 < t->sched_info) { */
/* 		printf("tcap %p with ndelegs %d, and current sched offset %d\n",  */
/* 		       t, t->ndelegs, t->sched_info); */
/* 		return -1; */
/* 	} */
/* 	for (i = 0 ; i < t->ndelegs ; i++) { */
/* 		if (!t->delegations[i].sched || */
/* 		    t->delegations[i].prio > 3) { */
/* 			printf("Invalid delegation in tcap:\n"); */
/* 			delegations_print(t); */
/* 			return -1; */
/* 		} */
/* 	} */
/* 	return 0; */
/* } */

/* #define LVLS 3 */

/* int  */
/* _pow(int b, int e) */
/* { */
/* 	int i; */
/* 	int v = b; */

/* 	for (i = 1 ; i < e ; i++) v *= b; */

/* 	return b; */
/* } */

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

/* void */
/* test_unit_matrix(int spdoff) */
/* { */
/* 	struct tcap *tcs[LVLS][LVLS*LVLS*LVLS]; */
/* 	struct thread thds[LVLS][LVLS*LVLS*LVLS]; */
/* #define get_spd(i) (&ss[(i+spdoff)%LVLS]) */
/* 	struct spd ss[LVLS]; */
/* 	struct tcap *roots[LVLS]; */
/* 	int i, j; */
/* #define get_prio(i) (i%LVLS) */
/* 	unsigned long long start, end; */
/* 	unsigned long long split_tot = 0, deleg_tot = 0, highest_tot = 0, merge_tot = 0; */
/* 	int split_cnt = 0, deleg_cnt = 0, highest_cnt = 0, merge_cnt = 1; */

/* 	for (i = 0 ; i < LVLS ; i++) { */
/* 		tcap_spd_init(get_spd(i)); */
/* 		roots[i] = tcap_get(get_spd(i), 0); */
/* 		assert(roots[i]); */
/* 	} */

/* 	for (i = 0 ; i < LVLS ; i++) { */
/* 		int upper = _pow(LVLS, i+1); */
/* 		rdtscll(start); */
/* 		for (j = 0 ; j < upper ; j++) { */
/* 			tcs[i][j] = tcap_split(roots[i], CYC_PLACEHOLDER, get_prio(j)); */
/* 			assert(tcs[i][j]); */
/* 			delegations_validate(tcs[i][j]); */
/* 		} */
/* 		rdtscll(end); */
/* 		split_tot += end-start; */
/* 		split_cnt += upper; */
/* 	} */

/* 	for (i = 1 ; i < LVLS ; i++) { */
/* 		int upper = _pow(LVLS, i+1); */
/* 		rdtscll(start); */
/* 		for (j = 0 ; j < upper ; j++) { */
/* 			//delegations_print(tcs[i-1][j/LVLS]); */
/* 			//delegations_print(tcs[i][j]); */
/* 			if (tcap_delegate(tcs[i][j], tcs[i-1][j/LVLS], */
/* 					  CYC_MIN, 0)) { */
/* 				printf("Cannot delegate from [%d][%d] \n", i-1, j/LVLS); */
/* 				delegations_print(tcs[i-1][j/LVLS]); */
/* 				printf("to [%d][%d]\n", i, j); */
/* 				delegations_print(tcs[i][j]); */
/* 				assert(0); */
/* 			} */
/* 			//delegations_validate(tcs[i][j]); */
/* 		}		 */
/* 		rdtscll(end); */
/* 		deleg_tot += end-start; */
/* 		deleg_cnt += upper; */
/* 	} */

/* 	for (i = 1 ; 0 && i < LVLS ; i++) { */
/* 		int upper = _pow(LVLS, i+1); */
/* 		rdtscll(start); */
/* 		for (j = 0 ; j < upper ; j++) { */
/* 			assert(!tcap_merge(tcs[i-1][j/(_pow(LVLS, i))],  */
/* 					   tcs[i][j])); */
/* 		}		 */
/* 		rdtscll(end); */
/* 		merge_tot += end-start; */
/* 		merge_cnt += upper; */
/* 	} */
	
/* 	printf("Cycle costs:  split %lld, delegate %lld, merge %lld\n",  */
/* 	       split_tot/split_cnt, deleg_tot/deleg_cnt, merge_tot/merge_cnt); */
/* } */

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
__transfer_is_legal(struct comps *cs, int *order, int off)
{
	int i;

	for (i = 1 ; i < off ; i++) {
		int o = order[i];
		printf("(%d <? %d), ", cs[o].prios[0], cs[o].prios[1]);
	}
	printf("\n");

	for (i = 1 ; i < off ; i++) {
		int o = order[i];
		if (cs[o].prios[0] > cs[o].prios[1]) return 1;
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
__print_specific(struct comps *cs, int *order, int n)
{
	int i, j;

	printf("Error case:\n{ .prios = {\n");
	for (i = 0 ; i < n ; i++) {
		printf("\t{.ps = {");
		for (j = 0 ; j < NCDELEG ; j++) {
			printf("%d%s", cs[i].prios[j], 
			       j == (NCDELEG-1) ? "" : ", ");
		}
		printf("}}%s\n", i == (n-1) ? "" : ",");
	}

	printf("   },\n  .order = {");
	for (i = 0 ; i < n ; i++) {
		printf("%d%s", order[i], i == (n-1) ? "},\n" : ", ");
	}
	printf("  .n = %d\n}\n", n);
}

#define tassert(p) do { if (!(p)) { __print_specific(cs, order, n); assert(p); }} while(0)

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

	for (i = 0 ; i < n ; i++) {	
		int o = order[i];
		
		printf("(%d, %d)\n", cs[o].prios[0], cs[o].prios[1]);
	}

	/* now the tests!...transfer: */
	for (i = 1 ; i < n ; i++) {
		int o = order[i];
		
		if (tcap_transfer(cs[o].ts[1], cs[o].ts[0], 0, 0)) {
			printf("*");
			tassert(__transfer_is_legal(cs, order, i));
		} else {
			printf(".");
			tassert(!__transfer_is_legal(cs, order, i));
		}
	}
	/* ...and higher priority... */
	for (i = 1 ; i < n ; i++) {
		int o = order[i];
		
		if (__tcap_higher_prio(cs[o].ts[1], cs[o].ts[0])) {
			printf("x");
			tassert(__transfer_is_legal(cs, order, i));
		} else {
			printf("X");
			tassert(!__transfer_is_legal(cs, order, i));
		}
	}

	for (i = 0 ; i < n ; i++) {
		tcap_spd_delete(&cs[i].c);
	}
}

void
test_fuzz_delegations(void)
{
	struct comps cs[NDELEGS];
	int order[NDELEGS], i;
	struct comps_summary tests[] = {
		{ .prios = {
				{.ps = {24204, 56189}},
				{.ps = {15529, 24865}},
				{.ps = {1912, 10889}},
				{.ps = {57464, 5652}},
				{.ps = {38727, 41552}},
				{.ps = {34570, 22348}}
			},
		  .order = {2, 4, 5, 0, 3, 1},
		  .n = 6
		},
		{ .n = 0 }
	};

	printf("Testing specific cases:\n");
	for (i = 0 ; tests[i].n ; i++) {
		__gen_specific(&tests[i], cs, order, tests[i].n);
		__test_delegations(cs, order, tests[i].n);
	}
	printf("Fuzz testing:\n");
	__gen_rand(cs, order, NDELEGS);
	__test_delegations(cs, order, NDELEGS);
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
//	test_unit_matrix(0);
//	test_unit_matrix(1);
//	test_unit_matrix(2);

	return 0;
}
