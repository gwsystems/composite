#ifdef TESTING

#include <stdio.h>
#include <malloc.h>

typedef unsigned short int spdid;
struct dep_edge;
struct protection_domain;
struct edge;

struct component {
	struct component *next;
	struct dep_edge *edges;
	struct protection_domain *pd;
	spdid_t id;
};

/* 
 * Protection domains are a network embedded on the component
 * dependency network that represent the protection domains of the
 * system (or the current configuration of the protection domains,
 * anyway).
 */
struct protection_domain {
	struct protection_domain *next;
	struct ipc_edge *edges;
	struct component *components;
};

/* base class for the protection_domain and component */
struct pd_base {
	struct pd_base *next;
	struct edge *edges;
};

/* edges between components designating dependency */
struct dep_edge {
	unsigned long invocations;
	struct component *from, *to;
	struct dep_edge *next;
};

/* edges between protection domains denoting IPC */
struct ipc_edge {
	unsigned long invocations;
	struct protection_domain *from, *to;
	struct dep_edge *next;
};

/* polymorphic base class */
struct edge {
	unsigned long invocations;
	struct edge *from, *to;
	struct edge *next;
};

static struct void mpd_component_init(struct component *c, spdid_t id)
{
	memset(c, 0, sizeof(struct component));
	c->id = id;
}

static struct void mpd_pd_init(struct protection_domain *pd)
{
	memset(pd, 0, sizeof(struct protection_domain));
}

static struct void mpd_edge_init(struct edge *e)
{
	memset(e, 0, sizeof(struct edge));
}

/* Adds an edge between from and to of size sizeof_edge */
static struct edge *mpd_add_edge(int sizeof_edge, 
				 struct base_pd *from, struct base_pd *to)
{
	struct edge *e = malloc(sizeof_edge);

	if (NULL == e) return NULL;
	
	e->from = from;
	e->to   = to;
	e->next = from->edges;
	from->edges = e;

	return e;
}

static inline struct dep_edge *mpd_add_dependency(struct component *c_from, struct component *c_to)
{
	return (struct dep_edge*)mpd_add_edge(sizeof(struct dep_edge), c_from, c_to);
}

static inline struct ipc_edge *mpd_add_ipc(struct protection_domain *pd_from, struct protection_domain *pd_to)
{
	return (struct ipc_edge*)mpd_add_edge(sizeof(struct ipc_edge), pd_from, pd_to);
}

static void mpd_merge_pd(struct protection_domain *pd_from, struct protection_domain *pd_to)
{
	struct ipc_edge *ie = pd_from->edge, *prev = NULL;
	struct component *c_to, *c_end = pd_from->components;

	/* there better be components in the protection domains */
	assert(c_end && pd_to->components);
	
	while (ie) {
		assert(ie->from = pd_from);
		
		if (ie->to == pd_to) {
			if (NULL == prev) {
				pd_from->edges = ie->to;
			} else {
				prev = ie->to;
			}
			free(ie);
			break;
		}
		
		prev = ie;
		ie = ie->next;
	}
	
	/* We better have found the edge between the domains */
	assert(ie);

	c_to = pd_to->components;
	while (c_end->next) c_end = c_end->next;
	c_end->next = c_to;
	pd_to->components = NULL;

	free(pd_to);

	return;
}

static int mpd_split_pd(struct protection_domain *pd, struct component *c)
{
	struct protection_domain *pd_new = malloc(sizeof(struct protection_domain));
	struct ipc_edge *ie;
	struct component *ipce, *prev = NULL;

	assert(pd->components);

	if (NULL == pd_new) {
		return -1;
	}
	ie = malloc(sizeof(struct ipc_edge));
	if (NULL == ie) {
		free(pd_new);
		return -1;
	}
	mpd_pd_init(pd_new);
	mpd_edge_init(ie);

	/* Take the component out of the protection domain */
	ipce = pd->components;
	if (ipce == c) {
		pd->components = c->next;
	} else {
		while (ipce->next != c) ipce = ipce->next;
		ipce->next = c->next;
	}
	c->next = NULL;
	pd_new->components = c;
	pd_new->edges = ie;
	ie->from = pd;
	ie->to = pd_new
}

#else 

#include <cos_component.h>
#include <cos_debug.h>
#include <cos_alloc.h>
#include <print.h>

extern int sched_block(spdid_t id);

/* Mirrored in cos_loader.c */
struct comp_graph {
	int client, server;
};
//struct comp_graph *graph;

static void mpd_loop(struct comp_graph *g)
{
	while (1) {
		sched_block(cos_spd_id());
		/* do MPD manipulations here! */
	}
	assert(0);
	return;
}

static void mpd_init(void)
{
	int i;
	struct comp_graph *graph;
	/* The hack to give this component the component graph is to
	 * place it @ cos_heap_ptr-PAGE_SIZE.  See cos_loader.c.
	 */
	struct comp_graph *g = (struct comp_graph *)cos_heap_ptr;
	
	graph = (struct comp_graph *)((char*)g-PAGE_SIZE);
	for (i = 0; graph[i].client && graph[i].server ; i++) {
		unsigned long amnt;
		amnt = cos_cap_cntl(graph[i].client, graph[i].server, 0);
		
		print("%d->%d w/ %d invocations.", graph[i].client, graph[i].server, (unsigned int)amnt);
	}
	mpd_loop(graph);
	assert(0);
	return;
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BOOTSTRAP:
		mpd_init();
		break;
	default:
		print("mpd_mgr: cos_upcall_fn error - type %x, arg1 %d, arg2 %d", 
		      (unsigned int)t, (unsigned int)arg1, (unsigned int)arg2);
		assert(0);
		return;
	}

	return;
}

#endif /* TESTING */
