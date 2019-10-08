#ifndef BARRIER_H
#define BARRIER_H

struct simple_barrier {
	unsigned long barrier;
	unsigned int ncore;
};

static void
simple_barrier(struct simple_barrier *b)
{
	unsigned long *barrier = &b->barrier;
	unsigned int ncore = b->ncore;

	assert(*barrier <= ncore);
	ps_faa(barrier, 1);
	while (ps_load(barrier) != ncore) ;
}

static inline void
simple_barrier_init(struct simple_barrier *b, unsigned int ncores)
{
	*b = (struct simple_barrier) {
		.barrier   = 0,
		.ncore     = ncores
	};
}

#define SIMPLE_BARRIER_INITVAL { .barrier = 0, .ncore = 1 }

#endif	/* BARRIER_H */
