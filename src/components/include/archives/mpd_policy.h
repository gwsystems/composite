/**
 * Copyright 2009 by Boston University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gabep1@cs.bu.edu, 2009
 */

#ifdef MPD_LINUX_TEST
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#define MAX_COMPONENTS 14
#else
#include <cos_component.h>
#include <cos_debug.h>
#include <cos_alloc.h>
#define MAX_COMPONENTS 512 // MAX_NUM_SPDS
#endif


#include <cos_list.h>
#include <cos_vect.h>
#include <heap.h>

//#define MPD_POLICY_DEBUG
#ifdef MPD_POLICY_DEBUG
#define debug(format, ...) printf(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

#ifndef MAX_LONG
#define MAX_LONG ((~(unsigned long)0) / 2)
#endif

// typedef unsigned short int spdid_t;
struct protection_domain;
struct component;
struct pd_edge;

struct edge {
	long              invocations;
	struct component *from, *to;
	struct edge *     from_next, *from_prev, *to_next, *to_prev;

	/* pd-related links (for list in pd_edge) */
	struct edge *   pd_next, *pd_prev;
	struct pd_edge *master;

	/* list of all edges */
	struct edge *next, *prev;
};

typedef enum { PDE_ON_HEAP = 1, PDE_OFF_HEAP } pd_edge_state_t;

struct pd_edge {
	pd_edge_state_t state;
	int             prio_q_idx;

	long                      weight, invocations;
	struct protection_domain *from, *to;
	struct pd_edge *          from_next, *from_prev, *to_next, *to_prev;

	/* list of edges that make up this link between pds */
	int         nedges;
	struct edge constituent_edges;

	struct pd_edge *next, *prev;
};

typedef enum { MOST_CONNECTED, LEAST_CONNECTED, SLAVE } min_cut_grp_t;

struct component {
	spdid_t                   id;
	struct edge               outward_edges, inward_edges;
	struct protection_domain *pd;
	struct component *        pd_next, *pd_prev;

	/******************************************************
	 * Data for the min-cut algorithm.  See "A Simple Min-Cut
	 * Algorithm" by Mechthild Stoer and Frank Wagner
	 */
	min_cut_grp_t grp;
	/* saving the cut-of-phase (best min-cut so far) */
	struct component *cop_next, *cop_prev;
	/*
	 * The id of the (merged) subgraph this component is part of,
	 * and its neighbor components.  Min cut alg finishes when all
	 * components in this protection domain are part of the
	 * subgraph.
	 */
	int               sg_nmembs;
	struct component *sg_master;
	struct component *sg_next, *sg_prev;
	/*
	 * Index into the priority queue, sorted by weight to the
	 * subgraph being computed (from the least connected, to the
	 * most).
	 */
	int  prio_q_idx;
	long edge_weight, edge_invocations;
	/******************************************************/

	/* general list of all components */
	struct component *next, *prev;
};

typedef enum { PD_ON_HEAP = 1, PD_OFF_HEAP } pd_state_t;

struct protection_domain {
	pd_state_t state;

	/* All members of the protection domain */
	int               nmembs;
	struct component *members;
	int               prio_q_idx;

	/* Min cut info */
	long              mc_amnt;
	struct component *mc_members;

	/* links to connect protection domains: */
	struct pd_edge outward_edges, inward_edges;

	/* list of all pds */
	struct protection_domain *next, *prev;
};

struct heaps {
	/*
	 * Heaps for computing the min-cut, for comparing the
	 * overheads of different protection domain's min-cuts, and
	 * for comparing the overheads of different edges separating
	 * protection domains.
	 */
	struct heap *mc_h, *pd_h, *pde_h;
};

/* FIXME: Small change -- all of these globals should be in a
 * structure passed to each relevant function instead of accessed
 * globally */
static int                      n_cs, n_pds;
static long                     tot_cost = 0, tot_inv = 0;
static struct component         cs;
static struct protection_domain pds;
static struct pd_edge           pdes;
static struct edge              es;
struct heaps                    hs;
COS_VECT_CREATE_STATIC(c_map);

static void
mpd_edge_init(struct edge *e)
{
	memset(e, 0, sizeof(struct edge));
	INIT_LIST(e, from_next, from_prev);
	INIT_LIST(e, to_next, to_prev);

	INIT_LIST(e, pd_next, pd_prev);
}

static void
pd_edge_init(struct pd_edge *e)
{
	memset(e, 0, sizeof(struct pd_edge));
	INIT_LIST(e, from_next, from_prev);
	INIT_LIST(e, to_next, to_prev);
	INIT_LIST(e, next, prev);

	mpd_edge_init(&e->constituent_edges);
}

static void
mpd_component_init(struct component *c, spdid_t id)
{
	memset(c, 0, sizeof(struct component));

	c->id        = id;
	c->sg_master = c;
	c->sg_nmembs = 1;
	INIT_LIST(c, sg_next, sg_prev);
	INIT_LIST(c, pd_next, pd_prev);
	INIT_LIST(c, cop_next, cop_prev);
	INIT_LIST(c, next, prev);
	mpd_edge_init(&c->outward_edges);
	mpd_edge_init(&c->inward_edges);

	ADD_LIST(&cs, c, next, prev);
}

static struct component *
mpd_component_alloc(spdid_t spdid, struct protection_domain *pd)
{
	struct component *c;

	assert(pd);
	assert(spdid > 0);

	c = malloc(sizeof(struct component));
	if (NULL == c) goto err;
	if (spdid != cos_vect_add_id(&c_map, (void *)c, spdid)) goto err_c;
	mpd_component_init(c, spdid);

	if (NULL == pd->members)
		pd->members = c;
	else
		ADD_LIST(pd->members, c, pd_next, pd_prev);
	pd->nmembs++;
	c->pd = pd;

	return c;
err_c:
	free(c);
err:
	return NULL;
}

static void
mpd_component_free(struct component *c)
{
	REM_LIST(c, sg_next, sg_prev);
	REM_LIST(c, pd_next, pd_prev);
	REM_LIST(c, cop_next, cop_prev);
	REM_LIST(c, next, prev);
	/* FIXME: free edges */
	cos_vect_del(&c_map, c->id);
	free(c);
}

static struct protection_domain *
pd_create(void)
{
	struct protection_domain *pd;

	pd = malloc(sizeof(struct protection_domain));
	if (NULL == pd) return NULL;
	memset(pd, 0, sizeof(struct protection_domain));

	debug("pd create %p\n", pd);

	pd->state      = PD_OFF_HEAP;
	pd->members    = NULL;
	pd->nmembs     = 0;
	pd->prio_q_idx = 0;
	INIT_LIST(pd, next, prev);
	ADD_LIST(&pds, pd, next, prev);

	pd_edge_init(&pd->outward_edges);
	pd_edge_init(&pd->inward_edges);

	n_pds++;

	return pd;
}

static inline long
pd_edge_get_weight(struct pd_edge *pde)
{
	assert(pde);

	return pde->weight;
}

static inline void
pd_edge_set_weight(struct pd_edge *pde, long val)
{
	assert(pde);

	tot_cost += val - pd_edge_get_weight(pde);
	pde->weight = val;
	if (pde->state == PDE_ON_HEAP) heap_adjust(hs.pde_h, pde->prio_q_idx);
}


static inline long
edge_get_inv(struct edge *e)
{
	return e->invocations;
}

static inline void
edge_set_inv(struct edge *e, long val)
{
	long prev = edge_get_inv(e);

	e->invocations = val;
	if (NULL != e->master) {
		long pd_prev = pd_edge_get_weight(e->master);
		long new     = pd_prev + (val - prev);

		assert(new >= 0);
		pd_edge_set_weight(e->master, new);
	}

	tot_inv += val - prev;
}

static void
pd_edge_free(struct pd_edge *pd_e)
{
	struct protection_domain *pd_from, *pd_to;
	//	struct edge *e, *n;

	assert(pd_e->nedges == 0);
	assert(pd_edge_get_weight(pd_e) == 0);
	pd_from = pd_e->from;
	pd_to   = pd_e->to;

	debug("remove pd edge %p from %p to %p\n", pd_e, pd_from, pd_to);

	assert(EMPTY_LIST(&pd_e->constituent_edges, pd_next, pd_prev));
	/* 		for (e = FIRST_LIST(&pd_e->constituent_edges, pd_next, pd_prev) ; */
	/* 		     e != &pd_e->constituent_edges ; ) { */
	/* 			n = FIRST_LIST(e, pd_next, pd_prev); */
	/* 			REM_LIST(e, pd_next, pd_prev); */
	/* 			e = n; */
	/* 		} */
	REM_LIST(pd_e, from_next, from_prev);
	REM_LIST(pd_e, to_next, to_prev);
	REM_LIST(pd_e, next, prev);

	if (pd_e->state == PDE_ON_HEAP) {
		assert(pd_e->prio_q_idx > 0);
		assert(hs.pde_h->data[pd_e->prio_q_idx] == pd_e);
		pd_e->state = PDE_OFF_HEAP;
		heap_remove(hs.pde_h, pd_e->prio_q_idx);
		assert(pd_e->prio_q_idx == 0);
	}

	free(pd_e);
}

static struct pd_edge *
pd_edge_create(struct protection_domain *pd_from, struct protection_domain *pd_to)
{
	struct pd_edge *pd_e;

	pd_e = malloc(sizeof(struct pd_edge));
	if (NULL == pd_e) return NULL;
	pd_edge_init(pd_e);
	ADD_LIST(&pdes, pd_e, next, prev);
	pd_e->from   = pd_from;
	pd_e->to     = pd_to;
	pd_e->nedges = 0;

	ADD_LIST(&pd_from->outward_edges, pd_e, to_next, to_prev);
	ADD_LIST(&pd_to->inward_edges, pd_e, from_next, from_prev);

	debug("create pd edge %p from %p->%p\n", pd_e, pd_from, pd_to);

	pd_e->state = PDE_ON_HEAP;
	heap_add(hs.pde_h, pd_e);

	return pd_e;
}

static void
pd_free(struct heaps *hs, struct protection_domain *pd)
{
	struct component *t;

	debug("pd free %p\n", pd);

	if (NULL != pd->members) {
		t = FIRST_LIST(pd->members, pd_next, pd_prev);
		do {
			t->pd = NULL;
			t     = FIRST_LIST(t, pd_next, pd_prev);
		} while (pd->members != t);
	}
	if (pd->state == PD_ON_HEAP) {
		assert(pd->prio_q_idx > 0);
		heap_remove(hs->pd_h, pd->prio_q_idx);
	}
	pd->mc_members = pd->members = NULL;
	REM_LIST(pd, next, prev);
	assert(EMPTY_LIST(&pd->outward_edges, to_next, to_prev));
	if (!EMPTY_LIST(&pd->inward_edges, from_next, from_prev)) {
		struct pd_edge *e;
		for (e = FIRST_LIST(&pd->inward_edges, from_next, from_prev); e != &pd->inward_edges;
		     e = FIRST_LIST(e, from_next, from_prev)) {
			debug("error: edge from %p -> %p\n", e->from, e->to);
		}
	}
	assert(EMPTY_LIST(&pd->inward_edges, from_next, from_prev));
	free(pd);

	n_pds--;
}

/*
 * Note that this will find an edge without respect to if pd1 or pd2
 * are "from" or "to".
 */
static struct pd_edge *
__pd_find_edge(struct protection_domain *pd1, struct protection_domain *pd2, int create)
{
	struct pd_edge *e, *pd_new;

	assert(pd1 && pd2);
	assert(pd1 != pd2);

	for (e = FIRST_LIST(&pd1->outward_edges, to_next, to_prev); e != &pd1->outward_edges;
	     e = FIRST_LIST(e, to_next, to_prev)) {
		if ((e->from == pd1 && e->to == pd2) || (e->from == pd2 && e->to == pd1)) return e;
	}
	for (e = FIRST_LIST(&pd1->inward_edges, from_next, from_prev); e != &pd1->inward_edges;
	     e = FIRST_LIST(e, from_next, from_prev)) {
		if ((e->from == pd1 && e->to == pd2) || (e->from == pd2 && e->to == pd1)) return e;
	}
	if (!create) return NULL;
	pd_new = pd_edge_create(pd1, pd2);
	if (NULL == pd_new) BUG();

	return pd_new;
}

static struct pd_edge *
pd_find_edge(struct protection_domain *pd1, struct protection_domain *pd2)
{
	return __pd_find_edge(pd1, pd2, 1);
}

static int
mpd_component_add_edge(struct component *from, struct component *to, long weight)
{
	struct edge *   e;
	struct pd_edge *pd_e;

	assert(from && to && from != to);
	assert(from->pd && to->pd && from->pd != to->pd);

	e = malloc(sizeof(struct edge));
	if (NULL == e) return -1;

	mpd_edge_init(e);
	e->from = from;
	e->to   = to;
	edge_set_inv(e, weight);
	ADD_LIST(&from->outward_edges, e, to_next, to_prev);
	ADD_LIST(&to->inward_edges, e, from_next, from_prev);
	ADD_LIST(&es, e, next, prev);

	if (from->pd == to->pd) return 0;

	/* if this is an edge between pds, connect them too */
	pd_e = pd_find_edge(from->pd, to->pd);
	ADD_LIST(&pd_e->constituent_edges, e, pd_next, pd_prev);
	pd_edge_set_weight(pd_e, pd_edge_get_weight(pd_e) + weight);
	pd_e->nedges++;
	e->master = pd_e;

	debug("add edge %p to pde %p\n", e, pd_e);

	return 0;
}

static inline void
pd_remove_pd_edge(struct edge *e)
{
	struct pd_edge *pd_e;

	/* If this edge doesn't cross protection domain boundaries, we're done. */
	if (NULL == e->master) {
		assert(EMPTY_LIST(e, pd_next, pd_prev));
		return;
	}
	pd_e = e->master;
	assert(!EMPTY_LIST(e, pd_next, pd_prev));
	REM_LIST(e, pd_next, pd_prev);

	assert(pd_e && pd_e->nedges > 0);

	debug("remove edge %p from pde %p\n", e, pd_e);
	pd_e->nedges--;
	pd_edge_set_weight(pd_e, pd_edge_get_weight(pd_e) - edge_get_inv(e));
	e->master = NULL;

	if (pd_e->nedges == 0) pd_edge_free(pd_e);
}

/*
 * If the pd is deallocated because it holds no more components,
 * return 1, otherwise 0.
 */
static int
pd_rem_component(struct heaps *hs, struct protection_domain *pd, struct component *c)
{
	struct edge *e;

	assert(c->pd == pd);
	assert(pd->nmembs > 0);

	/* remove from the list of members of the protection domain */
	if (pd->members == c) {
		pd->members = FIRST_LIST(c, pd_next, pd_prev);
		/* if c is the only member on the list, NULL out */
		if (pd->members == c) pd->members = NULL;
	}
	REM_LIST(c, pd_next, pd_prev);

	for (e = FIRST_LIST(&c->outward_edges, to_next, to_prev); e != &c->outward_edges;
	     e = FIRST_LIST(e, to_next, to_prev)) {
		pd_remove_pd_edge(e);
	}
	for (e = FIRST_LIST(&c->inward_edges, from_next, from_prev); e != &c->inward_edges;
	     e = FIRST_LIST(e, from_next, from_prev)) {
		pd_remove_pd_edge(e);
	}

	debug("pd %p, remove component %d\n", pd, c->id);

	c->pd = NULL;
	REM_LIST(c, cop_next, cop_prev);
	c->sg_master = NULL;
	c->sg_nmembs = 0;
	REM_LIST(c, sg_next, sg_prev);

	pd->nmembs--;
	if (pd->nmembs == 0) {
		pd_free(hs, pd);
		return 1;
	}
	if (pd->mc_members == c) pd->mc_members = NULL;
	return 0;
}

static inline void
pd_add_edge(struct edge *e, struct protection_domain *from, struct protection_domain *to)
{
	struct pd_edge *pd_e;

	assert(e->from->pd == from && e->to->pd == to);
	pd_e = pd_find_edge(from, to);
	ADD_LIST(&pd_e->constituent_edges, e, pd_next, pd_prev);
	pd_edge_set_weight(pd_e, pd_edge_get_weight(pd_e) + edge_get_inv(e));
	pd_e->nedges++;
	e->master = pd_e;
}

static void
pd_add_component(struct protection_domain *pd, struct component *c)
{
	struct edge *e;

	assert(pd && c);
	assert(pd != c->pd);

	c->pd = pd;
	pd->nmembs++;
	debug("add component %d to pd %p w/ refcnt %d\n", c->id, pd, pd->nmembs);
	assert(pd->nmembs > 0);

	if (NULL == pd->members) {
		pd->members = c;
		INIT_LIST(c, pd_next, pd_prev);
	} else {
		ADD_LIST(pd->members, c, pd_next, pd_prev);
	}

	for (e = FIRST_LIST(&c->outward_edges, to_next, to_prev); e != &c->outward_edges;
	     e = FIRST_LIST(e, to_next, to_prev)) {
		assert(e->to);
		if (pd != e->to->pd) pd_add_edge(e, pd, e->to->pd);
	}
	for (e = FIRST_LIST(&c->inward_edges, from_next, from_prev); e != &c->inward_edges;
	     e = FIRST_LIST(e, from_next, from_prev)) {
		assert(e->from);
		if (pd != e->from->pd) pd_add_edge(e, e->from->pd, pd);
	}

	INIT_LIST(c, cop_next, cop_prev);
	c->sg_master = NULL;
	c->sg_nmembs = 0;
	INIT_LIST(c, sg_next, sg_prev);
}

static struct component *mc_find_min_cut(struct protection_domain *pd, struct heap *mc_h);

static void
__pd_merge(struct protection_domain *pd, struct protection_domain *pd_fin, struct heaps *hs)
{
	struct component *c, *c_first, *c_fin_first;
	int               done = 0;

	assert(pd_fin && pd);
	assert(pd_fin->members && pd->members);

	c_fin_first = c = FIRST_LIST(pd_fin->members, pd_next, pd_prev);
	c_first         = FIRST_LIST(pd->members, pd_next, pd_prev);
	while (!done) {
		struct component *n;

		n = FIRST_LIST(c, pd_next, pd_prev);

		done = pd_rem_component(hs, pd_fin, c);
		pd_add_component(pd, c);

		assert(done || n != c);
		c = n;
	}

	mc_find_min_cut(pd, hs->mc_h);

	merge_w_err(c_fin_first->id, c_first->id);
}

static void
pd_merge(struct pd_edge *pd_e, struct heaps *hs)
{
	struct protection_domain *pd, *pd_fin;

	assert(pd_e);
	pd     = pd_e->from;
	pd_fin = pd_e->to;
	assert(pd != pd_fin);

	__pd_merge(pd, pd_fin, hs);
}

/*
 * Assume that the min-cut algorithm has been run on pd, and the
 * mc_members subset of the protection domain is defined.  The split
 * will be done along this sub-group.
 */
static struct protection_domain *
pd_split(struct protection_domain *pd, struct heaps *hs)
{
	struct protection_domain *pd_new;
	struct component *        c, *c_rep;

	assert(heap_empty(hs->mc_h));
	assert(pd && hs->mc_h);
	assert(pd->mc_members);

	debug("min-cut value %ld\n", pd->mc_amnt);

	pd_new = pd_create();
	if (NULL == pd_new) BUG(); // return NULL;

	/* move components from the min-cut group to the new pd */
	c_rep = c = FIRST_LIST(pd->mc_members, cop_next, cop_prev);
	while (1) {
		struct component *n;

		n = FIRST_LIST(c, cop_next, cop_prev);

		/* The min-cut cannot be the whole component! */
		if (pd_rem_component(hs, pd, c)) BUG();
		pd_add_component(pd_new, c);

		split_w_err(c->id, c->id);
		if (c_rep != c) merge_w_err(c_rep->id, c->id);

		if (c == n) break;
		c = n;
	}

	assert(heap_empty(hs->mc_h));
	mc_find_min_cut(pd, hs->mc_h);
	assert(heap_empty(hs->mc_h));
	mc_find_min_cut(pd_new, hs->mc_h);

	return pd_new;
}

/*** Code to implement the min cut algorithm ***
 *
 * "A Simple Min-Cut Algorithm" by Mechthild Stoer and Frank Wagner
 */

static inline int
pd_edge_internal(struct edge *e)
{
	assert(e && e->to && e->from && e->to->pd && e->from->pd);

	return e->to->pd == e->from->pd;
}

static long
component_grp_weight(struct component *s, struct component *grp)
{
	struct component *master;
	struct edge *     e;
	long              cnt = 0;

	assert(s && grp);

	master = grp->sg_master;
	assert(master);

	for (e = FIRST_LIST(&s->outward_edges, to_next, to_prev); e != &s->outward_edges;
	     e = FIRST_LIST(e, to_next, to_prev)) {
		assert(e->to);
		if (pd_edge_internal(e) && e->to->sg_master == master) cnt += edge_get_inv(e);
	}
	for (e = FIRST_LIST(&s->inward_edges, from_next, from_prev); e != &s->inward_edges;
	     e = FIRST_LIST(e, from_next, from_prev)) {
		assert(e->from);
		if (pd_edge_internal(e) && e->from->sg_master == master) cnt += edge_get_inv(e);
	}

	return cnt;
}

static long
component_grp_to_grp_weight(struct component *g1, struct component *g2)
{
	//	struct protection_domain *pd;
	struct component *c;
	long              cnt = 0;
	//	unsigned long g1_nmembs = 0, g2_nmembs = 0;
	//	unsigned long delta_fault_exposure, ret;

	c = g1;
	do {
		cnt += component_grp_weight(c, g2);
		c = FIRST_LIST(c, sg_next, sg_prev);
		//		g1_nmembs++;
	} while (c != g1);

	/* 	pd = g1->pd; */
	/* 	assert(g1->pd == g2->pd); */
	/* 	assert(pd->nmembs > 0 && amnt > 0); */
	/* 	assert(g2_nmembs <= pd->nmembs); */
	/* 	g2_nmembs = pd->nmembs - g1_nmembs; */

	/* 	delta_fault_exposure = 2 * g2_nmembs * g1_nmembs; */
	/* 	ret = cnt/delta_fault_exposure; */
	/* 	return (long)ret; */
	return cnt;
}

static void
__component_heap_adjust(struct component *c, struct edge *e, struct heap *h)
{
	if (LEAST_CONNECTED == c->grp) {
		c->edge_weight += edge_get_inv(e);
		heap_adjust(h, c->prio_q_idx);
	}
}

static void
component_adjust_heap(struct component *s, struct heap *h)
{
	struct edge *e;

	assert(s && h);

	for (e = FIRST_LIST(&s->outward_edges, to_next, to_prev); e != &s->outward_edges;
	     e = FIRST_LIST(e, to_next, to_prev)) {
		if (pd_edge_internal(e)) {
			struct component *c;

			assert(e->to);
			assert(e->to->sg_master);
			c = e->to->sg_master;
			__component_heap_adjust(c, e, h);
		}
	}
	for (e = FIRST_LIST(&s->inward_edges, from_next, from_prev); e != &s->inward_edges;
	     e = FIRST_LIST(e, from_next, from_prev)) {
		if (pd_edge_internal(e)) {
			struct component *c;

			assert(e->from);
			assert(e->from->sg_master);
			c = e->from->sg_master;
			__component_heap_adjust(c, e, h);
		}
	}
}

static void
component_grp_adjust_heap(struct component *g, struct heap *h)
{
	struct component *c, *m;

	m = g->sg_master;
	c = g;
	do {
		assert(c->sg_master == m);
		component_adjust_heap(c, h);
		c = FIRST_LIST(c, sg_next, sg_prev);
	} while (c != g);
}

static inline long
mc_calculate_value(struct component *mc, struct protection_domain *pd)
{
	assert(mc == mc->sg_master && mc->pd == pd);
	/*
	 * Could use mc->sg_nmembs and pd->nmembs,
	 * e.g. mc->edge_weight/(2*mc->sg_nmembs*pd->nmembs)
	 */
	return mc->edge_weight;
}

/*
 * Take an empty heap and all components.  Return the weight of the
 * cut of phase, the second to last most connected component, and the
 * last component that defines the minimum cut of phase.
 */
static long
mc_phase(struct heap *h, struct protection_domain *pd, struct component **second_to_last, struct component **mcop)
{
	struct component *s, *c, *t;

	assert(0 == heap_size(h)); /* init w/ empty heap! */
	assert(pd && h);

	s = pd->members;
	assert(s);
	s = s->sg_master;
	assert(s);
	s->grp = MOST_CONNECTED;

	/* Add all components/subgraphs to the heap */
	for (t = FIRST_LIST(s, pd_next, pd_prev); t != s; t = FIRST_LIST(t, pd_next, pd_prev)) {
		assert(t != s);
		if (t->sg_master == t) {
			t->grp         = LEAST_CONNECTED;
			t->edge_weight = component_grp_to_grp_weight(s, t);
			heap_add(h, t);
		} else {
			t->grp = SLAVE;
		}
	}
	assert(s->grp == MOST_CONNECTED);

	/* There is only one group left!  We are done */
	if (0 == heap_size(h)) {
		*second_to_last = NULL;
		*mcop           = NULL;
		return 0;
	}

	/*
	 * Keep adding the most tightly connected node (head of the
	 * prio q) to the MOST_CONNECTED set until we only have one
	 * node left.
	 */
	c = NULL;
	while (1 < heap_size(h)) {
		c      = heap_highest(h);
		c->grp = MOST_CONNECTED;
		component_grp_adjust_heap(c, h);
	}
	assert(1 == heap_size(h));
	*mcop           = heap_highest(h);
	*second_to_last = c ? c : s;

	assert(*mcop);

	return mc_calculate_value(*mcop, pd);
}

/* remove the current cut of phase, we found a better */
static void
mc_reset_cop(struct protection_domain *pd)
{
	struct component *t;

	assert(pd && pd->mc_members);

	t = pd->mc_members;
	while (1) {
		struct component *n;

		n = FIRST_LIST(t, cop_next, cop_prev);
		REM_LIST(t, cop_next, cop_prev);
		if (t == n) break;
		t = n;
	}
	pd->mc_members = NULL;
	pd->mc_amnt    = MAX_LONG;
}

static void
mc_append_lists(struct component *a, struct component *b)
{
	struct component *t;

	assert(a && b);
	assert(a == a->sg_master && b == b->sg_master);

	a->sg_nmembs += b->sg_nmembs;
	b->sg_nmembs = 0;

	t = b;
	while (1) {
		struct component *n;

		n = FIRST_LIST(t, sg_next, sg_prev);
		REM_LIST(t, sg_next, sg_prev);
		t->sg_master = a;
		ADD_LIST(a, t, sg_next, sg_prev);
		t->grp = SLAVE;
		if (t == n) break;
		t = n;
	}
	assert(b->sg_master == a);
}

static void
mc_prepare_data(struct protection_domain *pd)
{
	struct component *t;

	assert(pd);
	assert(pd->members);
	t = pd->members;
	do {
		REM_LIST(t, sg_next, sg_prev);
		INIT_LIST(t, sg_next, sg_prev);
		REM_LIST(t, cop_next, cop_prev);
		INIT_LIST(t, cop_next, cop_prev);
		t->sg_master   = t;
		t->sg_nmembs   = 1;
		t->edge_weight = 0;
		t->grp         = LEAST_CONNECTED;
		assert(pd == t->pd);
		t = FIRST_LIST(t, pd_next, pd_prev);
	} while (t != pd->members);
	pd->mc_members = NULL;
	pd->mc_amnt    = MAX_LONG;
}

/*
 * return a component that is part of the min-cut group (all of which
 * can be accessed via cop_{next|prev}
 */
static struct component *
mc_find_min_cut(struct protection_domain *pd, struct heap *h)
{
	long min_cut = MAX_LONG, curr_mc;
	/* min-cut of phase, and the second to least connected */
	struct component *mcop, *second_to_mcop;

	assert(pd && h);
	assert(pd->nmembs > 0 && pd->members);

	if (1 == pd->nmembs) {
		pd->mc_members = NULL;
		pd->mc_amnt    = MAX_LONG;
		/* if the pd is in the pd heap, remove it */
		if (pd->state == PD_ON_HEAP) {
			assert(pd->prio_q_idx > 0);
			pd->state = PD_OFF_HEAP;
			heap_remove(hs.pd_h, pd->prio_q_idx);
			pd->prio_q_idx = 0;
		}
		return NULL;
	}

	mc_prepare_data(pd);
	do {
		second_to_mcop = mcop = NULL;
		curr_mc               = mc_phase(h, pd, &second_to_mcop, &mcop);
		if (NULL == mcop) break;
		assert(NULL != second_to_mcop);
		mcop = mcop->sg_master;

		/* new best min-cut? */
		if (curr_mc < min_cut) {
			struct component *t, *n;

			if (pd->mc_members) mc_reset_cop(pd);
			t = mcop;
			n = FIRST_LIST(t, sg_next, sg_prev);
			while (t != n) {
				ADD_LIST(t, n, cop_next, cop_prev);
				n = FIRST_LIST(n, sg_next, sg_prev);
			}
			pd->mc_members = mcop;
			pd->mc_amnt    = curr_mc;
			min_cut        = curr_mc;
		}

		/* join the cut-of-phase with the second to least connected subgraph */
		mc_append_lists(mcop, second_to_mcop);
	} while (NULL != mcop);

	assert(pd->mc_members);
	/* if the pd isn't in the pd heap, add it, otherwise change its weight */
	if (pd->state == PD_OFF_HEAP) {
		assert(pd->prio_q_idx == 0);
		pd->state = PD_ON_HEAP;
		heap_add(hs.pd_h, pd);
	} else {
		assert(pd->prio_q_idx > 0);
		heap_adjust(hs.pd_h, pd->prio_q_idx);
	}

	return pd->mc_members;
}

/*** Code to setup the component graph: ***/

/* struct comp_graph { */
/* 	int client, server; */
/* }; */

static void
try_create_component(spdid_t id)
{
	if (!cos_vect_lookup(&c_map, id)) {
		struct protection_domain *pd;

		pd = pd_create();
		assert(pd);
		mpd_component_alloc(id, pd);
		n_cs++;
	}
}

static void
create_components(struct comp_graph *ies)
{
	int i;

	/* Create all components */
	for (i = 0; ies[i].client && ies[i].server; i++) {
		try_create_component(ies[i].client);
		try_create_component(ies[i].server);
	}

	/* add edges */
	for (i = 0; ies[i].client && ies[i].server; i++) {
		struct component *from, *to;

		from = cos_vect_lookup(&c_map, ies[i].client);
		assert(from);
		to = cos_vect_lookup(&c_map, ies[i].server);
		assert(to);

		if (mpd_component_add_edge(from, to, 0)) BUG();
	}
}

/* heap functions for min cut in a pd */
static int
mc_cmp(void *a, void *b)
{
	struct component *c1 = a, *c2 = b;

	return c1->edge_weight >= c2->edge_weight;
}

static void
mc_update(void *a, int pos)
{
	((struct component *)a)->prio_q_idx = pos;
}

/* heap functions for the collection of min-cut edges for all pds */
static int
pd_cmp(void *a, void *b)
{
	struct protection_domain *pd1 = a, *pd2 = b;

	return pd1->mc_amnt <= pd2->mc_amnt;
}

static void
pd_update(void *a, int pos)
{
	struct protection_domain *p = a;
	p->prio_q_idx               = pos;
}

/* heap functions for the collection of edges between protection domains */
static int
pde_cmp(void *a, void *b)
{
	struct pd_edge *e1 = a, *e2 = b;

	return e1->weight >= e2->weight;
}

static void
pde_update(void *a, int pos)
{
	struct pd_edge *e = a;

	e->prio_q_idx = pos;
}

void
mpd_pol_init(void)
{
	mpd_component_init(&cs, 0);
	cos_vect_init_static(&c_map);
	INIT_LIST(&pds, next, prev);
	INIT_LIST(&cs, next, prev);
	INIT_LIST(&pdes, next, prev);
	INIT_LIST(&es, next, prev);

	/* heap to be used in computing the min-cut for protection domains */
	struct heap *mc_h = heap_alloc(MAX_COMPONENTS, mc_cmp, mc_update);
	/* heap for the protection domains themselves ordered by mincut value */
	struct heap *pd_h  = heap_alloc(MAX_COMPONENTS, pd_cmp, pd_update);
	struct heap *pde_h = heap_alloc(MAX_COMPONENTS, pde_cmp, pde_update);

	assert(mc_h && pd_h && pde_h);
	assert(EMPTY_LIST(&pdes, next, prev));

	hs.mc_h  = mc_h;
	hs.pd_h  = pd_h;
	hs.pde_h = pde_h;
}

/*
 * What follows is the interface to the mpd functionality.
 *
 * After the components are edges are created, these can be used to
 * manipulate the tradeoff between isolation and performance.
 */

static void
mpd_increase_isolation(struct heaps *hs)
{
	struct protection_domain *pd;

	assert(hs && hs->pd_h);
	assert(heap_size(hs->pd_h) > 0);
	pd             = heap_highest(hs->pd_h);
	pd->state      = PD_OFF_HEAP;
	pd->prio_q_idx = 0;
	if (!pd_split(pd, hs)) BUG();
}

static void
mpd_decrease_overhead(struct heaps *hs)
{
	struct pd_edge *pde;

	assert(hs && hs->pde_h);
	assert(heap_size(hs->pde_h) > 0);
	pde             = heap_highest(hs->pde_h);
	pde->state      = PDE_OFF_HEAP;
	pde->prio_q_idx = 0;
	pd_merge(pde, hs);
}

static long
mpd_peek_inc_isolation(struct heaps *hs)
{
	assert(hs && hs->pd_h);
	assert(heap_size(hs->pd_h) > 0);
	return ((struct protection_domain *)heap_peek(hs->pd_h))->mc_amnt;
}

static long
mpd_peek_dec_overhead(struct heaps *hs)
{
	assert(hs && hs->pde_h);
	assert(heap_size(hs->pde_h) > 0);
	return ((struct pd_edge *)heap_peek(hs->pde_h))->weight;
}

static int
mpd_empty_inc_isolation(struct heaps *hs)
{
	assert(hs && hs->pd_h);
	return heap_empty(hs->pd_h);
}

static int
mpd_empty_dec_overhead(struct heaps *hs)
{
	assert(hs && hs->pde_h);
	return heap_empty(hs->pde_h);
}

static void
recompute_min_cuts(struct protection_domain *pds, struct heaps *hs)
{
	struct protection_domain *pd;

	for (pd = FIRST_LIST(pds, next, prev); pd != pds; pd = FIRST_LIST(pd, next, prev)) {
		mc_find_min_cut(pd, hs->mc_h);
	}
}

#ifdef TEST_EDGE_CONSISTENCY
static void
test_edge_weight_consistency(void)
{
	struct edge *   e;
	struct pd_edge *pd_e;

	for (pd_e = FIRST_LIST(&pdes, next, prev); pd_e != &pdes; pd_e = FIRST_LIST(pd_e, next, prev)) {
		int total = 0;
		for (e = FIRST_LIST(&pd_e->constituent_edges, pd_next, pd_prev); e != &pd_e->constituent_edges;
		     e = FIRST_LIST(e, pd_next, pd_prev)) {
			total += edge_get_inv(e);
		}
		assert(total == pd_edge_get_weight(pd_e));
	}
}
#else
#define test_edge_weight_consistency()
#endif

void
customize_overhead_to_limit(int allowed_invs)
{
	/* 	printf("dec oh\n"); */
	/* 	while (!mpd_empty_dec_overhead(&hs)) { */
	/* 		printf("\t%ld\n", mpd_peek_dec_overhead(&hs)); */
	/* 		mpd_decrease_overhead(&hs); */
	/* 	} */
	/* 	recompute_min_cuts(&pds, &hs); */
	/* 	printf("inc iso\n"); */
	/* 	while (!mpd_empty_inc_isolation(&hs)) { */
	/* 		printf("\t%ld\n", mpd_peek_inc_isolation(&hs)); */
	/* 		mpd_increase_isolation(&hs); */
	/* 	} */
	/* 	return; */

	while (!mpd_empty_dec_overhead(&hs) && tot_cost > allowed_invs) {
		mpd_decrease_overhead(&hs);
	}
	test_edge_weight_consistency();
	recompute_min_cuts(&pds, &hs);
	test_edge_weight_consistency();
	while (!mpd_empty_inc_isolation(&hs) && tot_cost + mpd_peek_inc_isolation(&hs) < allowed_invs) {
		mpd_increase_isolation(&hs);
	}
	test_edge_weight_consistency();
	while (!mpd_empty_dec_overhead(&hs) && !mpd_empty_inc_isolation(&hs)
	       && mpd_peek_inc_isolation(&hs) < mpd_peek_dec_overhead(&hs)) {
		long inc, dec;

		inc = mpd_peek_inc_isolation(&hs);
		dec = mpd_peek_dec_overhead(&hs);

		mpd_decrease_overhead(&hs);
		test_edge_weight_consistency();
		while (!mpd_empty_inc_isolation(&hs) && tot_cost + mpd_peek_inc_isolation(&hs) < allowed_invs) {
			mpd_increase_isolation(&hs);
			test_edge_weight_consistency();
		}

		/* got into a loop */
		if (inc == mpd_peek_inc_isolation(&hs) && dec == mpd_peek_dec_overhead(&hs)) break;
	}
}

void
remove_overhead_to_limit(int limit)
{
	while (!mpd_empty_dec_overhead(&hs) && tot_cost > limit) {
		mpd_decrease_overhead(&hs);
	}
}

int
remove_one_isolation_boundary(void)
{
	if (mpd_empty_dec_overhead(&hs)) return 1;
	mpd_decrease_overhead(&hs);
	return 0;
}

#ifdef MPD_LINUX_TEST

static void
merge_all_components(struct component *cs, struct heaps *hs)
{
	struct component *c, *s;

	s = FIRST_LIST(cs, next, prev);
	assert(s != cs);
	c = FIRST_LIST(s, next, prev);
	while (c != cs) {
		if (s->pd != c->pd) __pd_merge(s->pd, c->pd, hs);
		c = FIRST_LIST(c, next, prev);
		test_edge_weight_consistency();
	}
}

static void
split_all_components(struct protection_domain *pds, struct heaps *hs)
{
	struct protection_domain *first;
	/* only one pd allowed! */
	assert(FIRST_LIST(pds, next, prev) == LAST_LIST(pds, next, prev) && !EMPTY_LIST(pds, next, prev));

	first = FIRST_LIST(pds, next, prev);
	mc_find_min_cut(first, hs->mc_h);
	assert(first->mc_members);

	while (1) {
		struct protection_domain *pd;

		pd = FIRST_LIST(pds, next, prev);
		if (pd == pds) return;
		while (pd->nmembs == 1) {
			pd = FIRST_LIST(pd, next, prev);
			if (pd == pds) return;
		}
		debug("splitting pd %p w/ %d membs\n", pd, pd->nmembs);
		assert(pd->nmembs > 1);
		pd_split(pd, hs);
		test_edge_weight_consistency();
	}
}

/* The example from the min-cut paper + 2 extra edges */
//#define MAX_COMPONENTS 14
static struct comp_graph ies[MAX_COMPONENTS + 1] = {{.client = 1, .server = 2}, {.client = 2, .server = 3},
                                                    {.client = 3, .server = 4}, {.client = 4, .server = 8},
                                                    {.client = 8, .server = 7}, {.client = 7, .server = 6},
                                                    {.client = 6, .server = 5}, {.client = 5, .server = 1},
                                                    {.client = 5, .server = 2}, {.client = 2, .server = 6},
                                                    {.client = 3, .server = 7}, {.client = 7, .server = 4},

                                                    {.client = 1, .server = 2}, {.client = 2, .server = 6},

                                                    {.client = 0, .server = 0}};

static void
edges_reset_random_weights(void)
{
	struct edge *e;

	for (e = FIRST_LIST(&es, next, prev); e != &es; e = FIRST_LIST(e, next, prev)) {
		edge_set_inv(e, rand() % 100);
		debug("e %d->%d: %lu\n", e->from->id, e->to->id, edge_get_inv(e));
	}
}

static void
merge_split_repeat(struct heaps *hs)
{
	const int TEST_ITER = 1000;
	int       i;

	for (i = 0; i < TEST_ITER; i++) {
		struct protection_domain *pd;
		for (pd = FIRST_LIST(&pds, next, prev); pd != &pds; pd = FIRST_LIST(pd, next, prev)) {
			assert(pd->nmembs == 1);
		}
		edges_reset_random_weights();
		debug("\nMerging:\n");
		merge_all_components(&cs, hs);
		assert(!EMPTY_LIST(&pds, next, prev));
		assert(FIRST_LIST(&pds, next, prev) == LAST_LIST(&pds, next, prev));
		assert(EMPTY_LIST(&pdes, next, prev));
		debug("\nSplitting:\n");
		split_all_components(&pds, hs);
	}
}

static long
sum_pde_cost(void)
{
	struct pd_edge *pd_e;
	long            tot = 0;

	for (pd_e = FIRST_LIST(&pdes, next, prev); pd_e != &pdes; pd_e = FIRST_LIST(pd_e, next, prev)) {
		tot += pd_edge_get_weight(pd_e);
	}
	return tot;
}

static long
sum_e_cost(void)
{
	struct edge *e;
	long         tot = 0;

	for (e = FIRST_LIST(&es, next, prev); e != &es; e = FIRST_LIST(e, next, prev)) {
		tot += edge_get_inv(e);
	}
	return tot;
}

static void
test_repeat(struct heaps *hs)
{
	int i;

	for (i = 0; i < 10; i++) {
		edges_reset_random_weights();
		remove_overhead_to_limit(300);
		test_edge_weight_consistency();
		printf("#cs %d, #pds %d, tot inv %ld, tot cost %ld\n", n_cs, n_pds, tot_inv, tot_cost);
	}
}

void
test_driver(void)
{
	create_components(ies);
	//	merge_split_repeat(&hs);
	test_repeat(&hs);
}

int
main(void)
{
	srand(time(NULL));

	mpd_pol_init();
	test_driver();

	return 0;
}

#endif
