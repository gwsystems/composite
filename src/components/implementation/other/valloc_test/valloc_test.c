#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <valloc.h>
#include <cos_alloc.h>

#define ITER 9
#define BASE_SZ 512

void cos_init(void)
{
	void *a, *b, *c;
	int i, j;
	void *allocs[ITER];

	a = valloc_alloc(cos_spd_id(), cos_spd_id(), 16);
	assert(a);
	b = valloc_alloc(cos_spd_id(), cos_spd_id(), 64);
	assert(b);
	c = valloc_alloc(cos_spd_id(), cos_spd_id(), 32);
	assert(c);
	printc("%p, %p, %p\n", a, b, c);
	valloc_free(cos_spd_id(), cos_spd_id(), b, 64);
	valloc_free(cos_spd_id(), cos_spd_id(), a, 16);
	valloc_free(cos_spd_id(), cos_spd_id(), c, 32);

	for (i = 0 ; i < ITER ; i++) {
		for (j = 0 ; j <= i ; j++) {
			allocs[j] = malloc(BASE_SZ*(j+1));
			printc("allocation @ %p\n", allocs[j]);
			assert(allocs[j]);
		}

		for (j = i ; j >= 0 ; j--) {
			printc("attempting free @ %p\n", allocs[j]);
			free(allocs[j]);
		}
	}
	for (i = 0 ; i < ITER ; i++) {
		for (j = 0 ; j <= i ; j++) {
			allocs[j] = malloc(BASE_SZ*(j+1));
			printc("allocation @ %p\n", allocs[j]);
			assert(allocs[j]);
		}

		for (j = 0 ; j <= i ; j++) {
			printc("attempting free @ %p\n", allocs[j]);
			free(allocs[j]);
		}
	}

	return;
}
