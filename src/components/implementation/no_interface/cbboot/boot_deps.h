#include <print.h>
#undef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0;} } while(0)

#include <mem_mgr.h>
#include <sched.h>
#include <cos_alloc.h>
#include <cobj_format.h>
#include <cbuf.h>

/* 
 * Abstraction layer around 1) synchronization, 2) scheduling and
 * thread creation, and 3) memory operations.  
 */

/* synchronization... */
#define LOCK()   if (sched_component_take(cos_spd_id())) BUG();
#define UNLOCK() if (sched_component_release(cos_spd_id())) BUG();

/* scheduling/thread operations... */
#define __sched_create_thread_default sched_create_thread_default

/* memory operations... */
#define CBUFS_PER_COMPONENT (1024)
#define CBBOOT_COMPONENT_CNT (5)
#define NUM_CBBOOT_CBUFS (CBUFS_PER_COMPONENT * CBBOOT_COMPONENT_CNT)
static cbuf_t an_array_of_cbufs[NUM_CBBOOT_CBUFS];
static int index = 0;

#include <cinfo.h>
#include <cos_vect.h>

static void
boot_deps_init(void) { return; }

static void
boot_deps_map_sect(spdid_t spdid, void *src_start, vaddr_t dest_start, int pages, int sect_id, int sects)
{
	cbuf_t cbid;
	vaddr_t dsrc = (vaddr_t)src_start;
	vaddr_t dest_daddr = dest_start;
	char *caddr;
	spdid_t b_spd;

	assert(pages > 0);
	caddr = cbuf_alloc_ext(pages * PAGE_SIZE, &cbid, CBUF_EXACTSZ);
	assert(caddr);
	an_array_of_cbufs[index++] = cbid; /* FIXME: track cbufs better */

	b_spd = cos_spd_id();
	while (pages-- > 0) {
		if (dsrc != (mman_alias_page(b_spd, (vaddr_t)caddr, b_spd, dsrc, MAPPING_RW))) BUG();
		dsrc += PAGE_SIZE;
		caddr += PAGE_SIZE;
	}
	if (dest_daddr != (cbuf_map_at(b_spd, cbid, spdid, dest_daddr | MAPPING_RW | 2))) BUG();
}

static void
boot_deps_save_hp(spdid_t spdid, void *hp)
{
	cinfo_add_heap_pointer(cos_spd_id(), spdid, hp);
}

static void
boot_deps_run(void) { return; }

