#include <print.h>
#include <mem_mgr.h>
#include <sched.h>
#include <cos_alloc.h>

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
#define __mman_get_page   mman_get_page
#define __mman_alias_page mman_alias_page

#include <cinfo.h>
#include <cos_vect.h>

COS_VECT_CREATE_STATIC(spd_info_addresses);

int
cinfo_map(spdid_t spdid, vaddr_t map_addr, spdid_t target)
{
	vaddr_t cinfo_addr;

	cinfo_addr = (vaddr_t)cos_vect_lookup(&spd_info_addresses, target);
	if (0 == cinfo_addr) return -1;
	if (map_addr != 
	    (__mman_alias_page(cos_spd_id(), cinfo_addr, spdid, map_addr))) {
		return -1;
	}

	return 0;
}

spdid_t
cinfo_get_spdid(int iter)
{
	if (iter > MAX_NUM_SPDS) return 0;
	if (hs[iter] == NULL) return 0;

	return hs[iter]->id;
}

static int boot_spd_set_symbs(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci);
static void
comp_info_record(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci)
{
	if (!cos_vect_lookup(&spd_info_addresses, spdid)) {
		boot_spd_set_symbs(h, spdid, ci);
		cos_vect_add_id(&spd_info_addresses, (void*)round_to_page(ci), spdid);
	}
}

static void
boot_deps_init(void)
{
	cos_vect_init_static(&spd_info_addresses);
}
