#include <cos_component.h>
#include <cos_debug.h>
#include <cos_alloc.h>

extern int sched_block(spdid_t id);

/* Mirrored in cos_loader.c */
struct comp_graph {
	int client, server;
};
struct comp_graph *graph;

static void mpd_init(void)
{
	int i;
	struct comp_graph *g = (struct comp_graph *)cos_heap_ptr;

	graph = (struct comp_graph *)((char*)g-PAGE_SIZE);
	for (i = 0; graph[i].client && graph[i].server ; i++) {
		unsigned long amnt;
		amnt = cos_cap_cntl(graph[i].client, graph[i].server, 0);

		print("%d->%d w/ %d invocations.", graph[i].client, graph[i].server, (unsigned int)amnt);
	}
	while (1) sched_block(cos_spd_id());
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
