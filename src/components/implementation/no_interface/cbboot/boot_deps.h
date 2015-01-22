#include <print.h>
#undef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0;} } while(0)

#include <mem_mgr.h>
#include <sched.h>
#include <cos_alloc.h>
#include <cobj_format.h>

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
#define __local_mman_get_page   mman_get_page
#define __local_mman_alias_page mman_alias_page

#include <cinfo.h>
#include <cos_vect.h>

static int boot_spd_set_symbs(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci);
static void
comp_info_record(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci)
{
	vaddr_t cinfo_addr = (vaddr_t)cinfo_alloc_page(cos_spd_id());
	if (cinfo_addr != __local_mman_alias_page(cos_spd_id(), (vaddr_t)round_to_page(ci), cinfo_spdid(cos_spd_id()), cinfo_addr, MAPPING_RW)) BUG();
	if (!cinfo_add(cos_spd_id(), spdid, cinfo_addr)) {
		boot_spd_set_symbs(h, spdid, ci);
	}
}

static void
boot_deps_init(void) { return; }

static void
boot_deps_run(void) { return; }

