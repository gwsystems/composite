/**
 * Copyright 2008 by Boston University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gabep1@cs.bu.edu, 2008
 */

#include <cos_component.h>
#include <cos_debug.h>
#include <cos_alloc.h>
#include <print.h>

/* Mirrored in cos_loader.c */
struct comp_graph {
	int client, server;
};
//struct comp_graph *graph;

static inline int 
split_w_err(spdid_t a, spdid_t b)
{
	if (cos_mpd_cntl(COS_MPD_SPLIT, a, b)) {
		printc("split of %d from %d failed. %d\n", b, a, 0);
		return -1;
	}
	return 0;
}

static inline int 
merge_w_err(spdid_t a, spdid_t b)
{
	if (cos_mpd_cntl(COS_MPD_MERGE, a, b)) {
		printc("merge of %d and %d failed. %d\n", a, b, 0);
		return -1;
	}
	return 0;
}

//ugly, but for the sake of expediency...
#include <mpd_policy.h>

#include <timed_blk.h>
#include <sched.h>
#include <cgraph.h>

static void 
mpd_report(void)
{
	struct edge *e;
	struct protection_domain *pd;

	printc("Capability Invocations:\t");
	for (e = FIRST_LIST(&es, next, prev) ; 
	     e != &es ;
	     e = FIRST_LIST(e, next, prev)) {
		if (!e->invocations) continue;
		printc("%d->%d:%ld, ", e->from->id, e->to->id, e->invocations);
	}
	printc("\n"); return;

	printc("\nProtection Domains:\t");
	for (pd = FIRST_LIST(&pds, next, prev) ; 
	     pd != &pds ; 
	     pd = FIRST_LIST(pd, next, prev)) {
		struct component *c;
		
		printc("(");
		c = pd->members;
		assert(c);
		do {
			printc("%d,", c->id);
			c = FIRST_LIST(c, pd_next, pd_prev);
		} while (c != pd->members);
		printc("); ");
	}
	printc("\n");
}

static void 
update_edge_weights(void)
{
	struct edge *e;
	
	for (e = FIRST_LIST(&es, next, prev) ; 
	     e != &es ;
	     e = FIRST_LIST(e, next, prev)) {
		unsigned long invs;

		invs = cos_cap_cntl_spds(e->from->id, e->to->id, 0);
		if (invs != (invs & 0x7FFFFFFF)) BUG();
		edge_set_inv(e, (long)invs);
	}
}

/* #e = %oh * 1000000 / e_oh = .1 * 1000000 / 0.7 = 142857 */
/* #e = %oh * 1000000 / e_oh = .2 * 1000000 / 0.7 = 285714 */
/* #e = %oh * 1000000 / e_oh = .3 * 1000000 / 0.7 = 428571 */
#define PERMITTED_EDGES (428571)
//#define PERMITTED_EDGES (285714)

static void 
mpd_pol_linear(void)
{
	static int cnt = 0;
	/* currently timeouts are expressed in ticks */
	timed_event_block(cos_spd_id(), 24);
	update_edge_weights();

	customize_overhead_to_limit((unsigned int)PERMITTED_EDGES/(unsigned int)4);
	if (cnt == 3) { cnt = 0; mpd_report(); }
	else cnt++;
}

static void 
mpd_pol_never_increase(void)
{
	static int cnt = 0;
	/* currently timeouts are expressed in ticks */
	timed_event_block(cos_spd_id(), 24);
	update_edge_weights();

	remove_overhead_to_limit((unsigned int)PERMITTED_EDGES/(unsigned int)4);
	if (cnt == 3) { cnt = 0; mpd_report(); }
	else cnt++;
}

static void 
mpd_pol_dec_isolation_by_one(void)
{
	/* currently timeouts are expressed in ticks */
	timed_event_block(cos_spd_id(), 98);
	update_edge_weights();
	remove_one_isolation_boundary();
	mpd_report();
}

static void 
mpd_pol_report(void)
{
	timed_event_block(cos_spd_id(), 198);
	update_edge_weights();
	mpd_report();
}

static void 
mpd_loop(struct comp_graph *g)
{
	while (1) {
		mpd_pol_report();
		//mpd_pol_linear();
		//mpd_pol_never_increase();
		//mpd_pol_dec_isolation_by_one();
	}
	BUG();
	return;
}

static unsigned long long merge_total = 0;
static int merge_cnt = 0;

static unsigned long 
merge_avg(void)
{
	return (unsigned long)(merge_total/merge_cnt);
}

static int 
merge_all(void)
{
	unsigned long long pre, post;
	/*
	  8->4=a
	  2->a=b
	  b->7=c
	  3->1=d
	  c->d=e
	  e->5=f
	  f->6=g
	*/
	rdtscll(pre);
	if (merge_w_err(8, 4)) return -1;
/* 	if (merge_w_err(2, 8)) return -1; */
/* 	if (merge_w_err(8, 7)) return -1; */
/* 	if (merge_w_err(3, 1)) return -1; */
/* 	if (merge_w_err(3, 8)) return -1; */
/* 	if (merge_w_err(8, 5)) return -1; */
/* 	if (merge_w_err(8, 6)) return -1; */
	rdtscll(post);

	merge_total += post - pre;
//	merge_cnt+=7;
	merge_cnt++;

	return 0;
}

static unsigned long long split_total = 0;
static int split_cnt = 0;

static unsigned long 
split_avg(void)
{
	return (unsigned long)(split_total/split_cnt);
}

static int 
split_all(void)
{
	unsigned long long pre, post;

	rdtscll(pre);
/*	if (split_w_err(8, 6)) return -1; */
/* 	if (split_w_err(8, 5)) return -1; */
/* 	if (split_w_err(8, 1)) return -1; */
/* 	if (split_w_err(8, 3)) return -1; */
/* 	if (split_w_err(8, 7)) return -1; */
/* 	if (split_w_err(8, 2)) return -1; */
 	if (split_w_err(8, 4)) return -1;
	rdtscll(post);

	split_total += post - pre;
//	split_cnt+=7;
	split_cnt++;

	return 0;
}

static void 
mpd_bench(void)
{
	int i;
	for (i = 0 ; i < 1000 ; i++) {
		merge_all();
		split_all();
	}

	printc("merge: %ld, split %ld.\n", merge_avg(), split_avg());
}

static void 
mpd_merge_selective(void)
{
	int ms[] = {1, 0};
	int i;

	if (ms[0] == 0) return;
	for (i = 1; ms[i] != 0 ; i++) {
		if (cos_mpd_cntl(COS_MPD_MERGE, ms[0], ms[i])) {
			printc("merge of %d and %d failed. %d\n", ms[i], ms[i], 0);
		}
	}

	update_edge_weights();
	mpd_report();
	timed_event_block(cos_spd_id(), 498);
	update_edge_weights();
	mpd_report();
	timed_event_block(cos_spd_id(), 498);
	update_edge_weights();
	mpd_report();
	timed_event_block(cos_spd_id(), 498);
	update_edge_weights();
	mpd_report();
	timed_event_block(cos_spd_id(), 498);
	update_edge_weights();
	mpd_report();
	timed_event_block(cos_spd_id(), 498);
	update_edge_weights();
	mpd_report();
	timed_event_block(cos_spd_id(), 498);
}

static void 
mpd_merge_all(struct comp_graph *g)
{
	int i;

	prints("Merging all protection domains!\n");
	for (i = 0 ; g[i].client && g[i].server ; i++) {
		if (cos_mpd_cntl(COS_MPD_MERGE, g[i].client, g[i].server)) {
			printc("merge of %d and %d failed. %d\n", g[i].client, g[i].server, 0);
		}
	}
	
	return;
}

struct comp_graph graph[512];
static void 
mpd_init(void)
{
	int i;
	for (i = 0 ; cgraph_server(i) >= 0 ; i++) {
		graph[i].client = cgraph_client(i);
		graph[i].server = cgraph_server(i);
		cos_cap_cntl_spds(graph[i].client, graph[i].server, 0);	
	}

	mpd_pol_init();
	create_components(graph);

	/* merge all components into one protection domain */
//	mpd_merge_all(graph);
//	mpd_merge_selective();
	/* remove protection domains on a time-trigger */
//	while (!remove_one_isolation_boundary()); /* merge all pds */
	/* Intelligently manage pds */
//	mpd_loop(graph);
	return;
}

void
cos_init(void *d)
{
	mpd_init();
}

void bin(void)
{
	sched_block(cos_spd_id(), 0);
}

