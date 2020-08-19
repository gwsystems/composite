#ifndef SIMPLE_RING_H
#define SIMPLE_RING_H

#include <cos_component.h>
#include <simple_slab.h>

/* Header for an entry in the ring */
struct sr_head {
	ss_state_t status;
	char   obj[0];
};

struct sr_ring {
	signed long  producer, consumer;
 	unsigned int nobj, objsz;
	char mem[0];
};

#define SR_MEM_SZ(nobj, objsz) (sizeof(struct sr_ring) + nobj * (sizeof(struct sr_head) + objsz))

static inline sr_head *
__sr_nth(struct sr_ring *r, unsigned int idx)
{
	unsigned int off = idx % r->nobj;

	return (struct sr_head *)(&r->mem[0] + (r->objsz + sizeof(struct sr_head)) * off);
}

static void
sr_init(struct sr_ring *r, unsigned int nobj, unsigned int objsz)
{
	*r = (struct sr_ring) {
		.producer = 0,
		.consumer = 0,
		.nobj = nobj,
		.objsz = objsz
	};
	memset(r->mem, 0, SR_MEM_SZ(nobj, objsz));
}

static void
sr_teardown(struct sr_ring *r)
{

}

static inline int
sr_dequeue(struct sr_ring *r, void *data)
{
	struct sr_head *h;
	unsigned long *cons = r->consumer;

	h = __sr_nth(r, );


}

#endif	/* SIMPLE_RING_H */
