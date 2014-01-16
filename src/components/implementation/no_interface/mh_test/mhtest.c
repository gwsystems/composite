#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <mem_mgr.h>

#define ITER 10000

void cos_init(void)
{
	u64_t map_tot = 0, map_max = 0, unmap_tot = 0, unmap_max = 0, start, end;
	int i;
	
	/* establish all caches */
	mman_get_page(cos_spd_id(), (vaddr_t)cos_get_heap_ptr(), MAPPING_RW);
	mman_release_page(cos_spd_id(), (vaddr_t)cos_get_heap_ptr(), 0);

	for (i = 0 ; i < ITER ; i++) {
		u64_t diff;

		rdtscll(start);
		mman_get_page(cos_spd_id(), (vaddr_t)cos_get_heap_ptr(), MAPPING_RW);
		rdtscll(end);
		diff = end-start;
		map_tot += diff;
		if (map_max < diff) map_max = diff;
		rdtscll(start);
		mman_release_page(cos_spd_id(), (vaddr_t)cos_get_heap_ptr(), 0);
		rdtscll(end);
		diff = end-start;
		unmap_tot += diff;
		if (unmap_max < diff) unmap_max = diff;
	}
	printc("map avg %lld, map max %lld, unmap avg %lld, unmap max %lld\n",
	       map_tot/(u64_t)ITER, map_max, unmap_tot/(u64_t)ITER, unmap_max);
}

void hack(void) { sched_block(cos_spd_id(), 0); }
