#include <resmgr.h>
#include <sl.h>
#include <res_spec.h>
#include <hypercall.h>

#define MAX_CHILD_COMPS 8
#define MAX_CHILD_BITS  64

u32_t cycs_per_usec = 0;
u64_t child_spdbits = 0;

static struct cos_defcompinfo child_defcinfo[MAX_CHILD_COMPS];
static unsigned int num_child;
static struct sl_thd *child_initthd[MAX_CHILD_COMPS] = { NULL };

#define FIXED_PRIO 1
#define IS_BIT_SET(v, pos) (v & ((u64_t)1 << pos))

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);
	int i;

	PRINTC("CPU cycles per sec: %u\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));

	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();

	sl_init(SL_MIN_PERIOD_US);
	memset(&child_defcinfo, 0, sizeof(struct cos_defcompinfo) * MAX_CHILD_COMPS);

	hypercall_comp_childspdids_get(cos_spd_id(), &child_spdbits);
	PRINTC("Child bitmap : %llx\n", child_spdbits);

	for (i = 0; i < MAX_CHILD_BITS; i++) {
		if (IS_BIT_SET(child_spdbits, i)) {
			PRINTC("Initializing child component %d\n", i + 1);
			cos_defcompinfo_childid_init(&child_defcinfo[num_child], i + 1);

			/* TODO: get more info. whether it's a scheduler or not!*/
			child_initthd[num_child] = sl_thd_child_initaep_alloc(&child_defcinfo[num_child], 0, 0);
			assert(child_initthd[num_child]);

			sl_thd_param_set(child_initthd[num_child], sched_param_pack(SCHEDP_PRIO, FIXED_PRIO));
			num_child ++;
		}
	}
	assert(num_child);
	hypercall_comp_init_done();

	sl_sched_loop_nonblock();

	PRINTC("ERROR: Should never have reached this point!!!\n");
	assert(0);
}

