#include <print.h>

#undef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0;} } while(0)

//#include <mem_mgr.h>
#include <sched.h>
#include <cos_alloc.h>
#include <cobj_format.h>
#include <cos_types.h>

static vaddr_t kmem_heap = BOOT_MEM_KM_BASE;
static unsigned long n_kern_memsets = 0;

//the booter uses this to keep track of each comp mapped in
struct comp_cap_info {
	struct cos_compinfo cos_compinfo;
	vaddr_t addr_start;
	vaddr_t vaddr_mapped_in_booter;
	vaddr_t upcall_entry;
};
struct comp_cap_info comp_cap_info[MAX_NUM_SPDS+1];

void
cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	cos_init();	
	return;
}

#define CAP_ID_32B_FREE BOOT_CAPTBL_FREE            // goes up
#define CAP_ID_64B_FREE ((PAGE_SIZE*BOOT_CAPTBL_NPAGES - PAGE_SIZE/2)/16 - CAP64B_IDSZ) // goes down

enum{
       	BOOT_SINV_CAP = round_up_to_pow2(CAP_ID_32B_FREE + CAP32B_IDSZ, CAPMAX_ENTRY_SZ)
};
