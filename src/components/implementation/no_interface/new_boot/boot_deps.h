#include <print.h>

#undef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0;} } while(0)

#include <cos_alloc.h>
#include <cobj_format.h>
#include <cos_types.h>

/*the booter uses this to keep track of each comp*/
struct comp_cap_info {
	struct cos_compinfo cos_compinfo;
	vaddr_t addr_start;
	vaddr_t vaddr_mapped_in_booter;
	vaddr_t upcall_entry;
};
struct comp_cap_info comp_cap_info[MAX_NUM_SPDS+1];

struct cos_compinfo boot_info;

thdcap_t schedule[MAX_NUM_SPDS+1];
unsigned int cur_sched;

static int
boot_comp_map_memory(struct cobj_header *h, spdid_t spdid, pgtblcap_t pt)
{
	int i, j;
	int flag;
	vaddr_t dest_daddr, prev_map = 0;
	int tot = 0, n_pte = 1;	
	
	struct cobj_sect *sect = cobj_sect_get(h, 0);
	
	/*Expand Page table, could do this faster*/
	for(j = 0; j < (int)h->nsect; j++){
		tot += cobj_sect_size(h, j);
	}
	printc("tot: %d\n", tot);
	if (tot > SERVICE_SIZE) {
		n_pte = tot / SERVICE_SIZE;
		if(tot % SERVICE_SIZE) n_pte++;
	}	
	for(j = 0; j < n_pte; j++){
		if (!__bump_mem_expand_range(&boot_info, pt, sect->vaddr, SERVICE_SIZE)) BUG();
	}

	/* We'll map the component into booter's heap. */
	comp_cap_info[spdid].vaddr_mapped_in_booter = (vaddr_t)cos_get_heap_ptr();
	
	for (i = 0 ; i < h->nsect ; i++) {
		int left;

		sect = cobj_sect_get(h, i);
		flag = MAPPING_RW;
		if (sect->flags & COBJ_SECT_KMEM) {
			flag |= MAPPING_KMEM;
		}

		dest_daddr = sect->vaddr;
		left       = cobj_sect_size(h, i);
		
		/* previous section overlaps with this one, don't remap! */
		if (round_to_page(dest_daddr) == prev_map) {
			left -= (prev_map + PAGE_SIZE - dest_daddr);
			dest_daddr = prev_map + PAGE_SIZE;
		}

		while (left > 0) {
			vaddr_t addr = cos_page_bump_alloc(&boot_info);
			assert(addr);
			
			if (cos_mem_alias_at(&comp_cap_info[spdid].cos_compinfo, dest_daddr, &boot_info, addr)) BUG();
			prev_map = dest_daddr;
			dest_daddr += PAGE_SIZE;
			left       -= PAGE_SIZE;
		}
	}

	return 0;
}

void
cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	static int first = 1;
	int i;
	
	printc("core %ld: <<cos_upcall_fn thd %d (type %d, CREATE=%d, DESTROY=%d, FAULT=%d)>>\n",
	       cos_cpuid(), cos_get_thd_id(), t, COS_UPCALL_THD_CREATE, COS_UPCALL_DESTROY, COS_UPCALL_UNHANDLED_FAULT);

	if (first) {
		first = 0;
		for(i = 0; i < MAX_NUM_SPDS; i++){
			schedule[i] = NULL;
		}
		__alloc_libc_initilize();
		constructors_execute();
		cur_sched = 0;
	}

	switch (t) {
	case COS_UPCALL_THD_CREATE:
	{
		/* arg1 is the thread init data. 0 means
		 * bootstrap. */
		if (arg1 == 0) {
			cos_init(NULL);
		}
	
		cos_thd_switch(schedule[cur_sched]);
	
		return;
	}
	default:
		/* fault! */
		*(int*)NULL = 0;
		return;
	}
	
	return;
}

#define CAP_ID_32B_FREE BOOT_CAPTBL_FREE            // goes up
capid_t capid_32b_free = CAP_ID_32B_FREE;

/*Macro for sinv back to booter from new component*/
enum{
       	BOOT_SINV_CAP = round_up_to_pow2(CAP_ID_32B_FREE + CAP32B_IDSZ, CAPMAX_ENTRY_SZ)
};


void
boot_thd_done(void)
{
	printc("\nWelcome back to the booter!\n");
	cur_sched++;

	if (schedule[cur_sched] != NULL) {
		printc("Initializing comp: %d\n", cur_sched);
	       	cos_thd_switch(schedule[cur_sched]);
	} else {
	       	printc("Done Initializing\n");
	}

	printc("What to do now... hm..\n");
}

