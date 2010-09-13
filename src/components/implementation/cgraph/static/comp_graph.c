/**
 * Copyright 2010 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2010
 */

#include <cos_component.h>
#include <sched.h>

#define NDYN_EDGES 1024

/* Mirrored in cos_loader.c */
struct comp_graph {
	int client, server;
};
int nedges;
struct comp_graph *graph;

int dyn_next = 0;
struct comp_graph dyn_graph[NDYN_EDGES];

int 
cgraph_server(int iter)
{
	if (iter < nedges) return graph[iter].server;
	iter -= nedges;
	if (iter < dyn_next) return dyn_graph[iter].server;
	return -1;
}

int 
cgraph_client(int iter)
{
	if (iter < nedges) return graph[iter].client;
	iter -= nedges;
	if (iter < dyn_next) return dyn_graph[iter].client;
	return -1;
}

int
cgraph_add(int serv, int client)
{
	struct comp_graph *g;

	if (dyn_next == NDYN_EDGES) return -1;
	g = &dyn_graph[dyn_next++];
	g->server = serv;
	g->client = client;
	
	return 0;
}

static void 
cgraph_init(void)
{
	int i;
	/* The hack to give this component the component graph is to
	 * place it @ cos_heap_ptr-PAGE_SIZE.  See cos_loader.c.
	 */
	struct comp_graph *g = (struct comp_graph *)cos_get_heap_ptr();

	graph = (struct comp_graph *)((char*)g-PAGE_SIZE);
	for (i = 0 ; graph[i].client && graph[i].server ; i++) {
		cos_cap_cntl_spds(graph[i].client, graph[i].server, 0);	
	}
	nedges = i;

	return;
}

void 
cos_init(void *d)
{
	cgraph_init();
}

void
bin(void)
{
	sched_block(cos_spd_id(), 0);
}

