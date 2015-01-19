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

COS_VECT_CREATE_STATIC(spd_info_addresses);

int
cinfo_add(spdid_t spdid, spdid_t target, struct cos_component_information *ci)
{
	if (cos_vect_lookup(&spd_info_addresses, target)) return -1;
	cos_vect_add_id(&spd_info_addresses, (void*)round_to_page(ci), target);
	return 0;
}

void*
cinfo_alloc_page(spdid_t spdid)
{
	void *p = cos_get_vas_page();
	return p;
}

int
cinfo_map(spdid_t spdid, vaddr_t map_addr, spdid_t target)
{
	vaddr_t cinfo_addr;

	cinfo_addr = (vaddr_t)cos_vect_lookup(&spd_info_addresses, target);
	if (0 == cinfo_addr) return -1;
	if (map_addr != 
	    (__local_mman_alias_page(cos_spd_id(), cinfo_addr, spdid, map_addr, MAPPING_RW))) {
		return -1;
	}

	return 0;
}

int
cinfo_spdid(spdid_t spdid)
{
	return cos_spd_id();
}

static int boot_spd_set_symbs(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci);
static void
comp_info_record(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci)
{
	if (!cinfo_add(cos_spd_id(), spdid, ci)) {
		boot_spd_set_symbs(h, spdid, ci);
	}
}

static void
boot_deps_init(void)
{
	cos_vect_init_static(&spd_info_addresses);
}

static void
boot_deps_run(void) { return; }

