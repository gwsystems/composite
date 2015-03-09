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

static vaddr_t
__local_mman_get_page(spdid_t spd, vaddr_t addr, int flags)
{
	cbuf_t cbid;
	void *caddr = cbuf_alloc(PAGE_SIZE, &cbid);
	assert(caddr);
	assert(index < NUM_CBBOOT_CBUFS);
	an_array_of_cbufs[index++] = cbid; /* FIXME: track cbufs better */
	return mman_alias_page(spd, (vaddr_t)caddr, spd, addr, flags);
}

static vaddr_t
__local_mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr, int flags)
{
	cbuf_t cbid = an_array_of_cbufs[index-1];
	// don't need to valloc_alloc_at(d_addr) because it is "reserved"...
	return cbuf_map_at(s_spd, cbid, d_spd, d_addr | flags | 2);
}

#include <cinfo.h>
#include <cos_vect.h>

static void
boot_deps_init(void) { return; }

static void
boot_deps_map_pages(spdid_t spdid, void *src_start, vaddr_t dest_start, int pages)
{
	char *dsrc = src_start;
	vaddr_t dest_daddr = dest_start;
	assert(pages > 0);
	while (pages-- > 0) {
		/* TODO: if use_kmem, we should allocate
		 * kernel-accessible memory, rather than
		 * normal user-memory */
		if ((vaddr_t)dsrc != __local_mman_get_page(cos_spd_id(), (vaddr_t)dsrc, MAPPING_RW)) BUG();
		if (dest_daddr != (__local_mman_alias_page(cos_spd_id(), (vaddr_t)dsrc, spdid, dest_daddr, MAPPING_RW))) BUG();
		dsrc += PAGE_SIZE;
		dest_daddr += PAGE_SIZE;
	}
	return 0;
}

static void
boot_deps_save_hp(spdid_t spdid, void *hp)
{
	cinfo_add_heap_pointer(cos_spd_id(), spdid, hp);
}

static void
boot_deps_run(void) { return; }

