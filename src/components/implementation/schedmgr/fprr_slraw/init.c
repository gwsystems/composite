#include <sl.h>
#include <res_spec.h>
#include <hypercall.h>

#define MAX_CHILD_COMPS 8
#define MAX_CHILD_BITS  64

u32_t cycs_per_usec = 0;
u64_t child_spdbits = 0;

struct child_info {
	struct cos_defcompinfo defcinfo;
	struct sl_thd *initaep;
	spdid_t spdid;
};
static struct child_info childinfo[MAX_CHILD_COMPS];
static int num_child = 0;

struct cos_defcompinfo *
child_defci_get(spdid_t spdid)
{
	int i;

	for (i = 0; i < num_child; i ++) {
		if (childinfo[i].spdid == spdid) return &(childinfo[i].defcinfo);
	}

	return NULL;
}

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
	memset(&childinfo, 0, sizeof(struct child_info) * MAX_CHILD_COMPS);

	hypercall_comp_childspdids_get(cos_spd_id(), &child_spdbits);
	PRINTC("Child bitmap : %llx\n", child_spdbits);

	for (i = 0; i < MAX_CHILD_BITS; i++) {
		if (IS_BIT_SET(child_spdbits, i)) {
			struct cos_defcompinfo *child_dci = &(childinfo[num_child].defcinfo);
			struct cos_compinfo *child_ci = cos_compinfo_get(child_dci);

			PRINTC("Initializing child component %d\n", i + 1);
			child_ci->comp_cap = hypercall_comp_compcap_get(i + 1);
			childinfo[num_child].spdid = i + 1;

			/* TODO: get more info. whether it's a scheduler or not!*/
			childinfo[num_child].initaep = sl_thd_child_initaep_alloc(child_dci, 0, 0);
			assert(childinfo[num_child].initaep);

			sl_thd_param_set(childinfo[num_child].initaep, sched_param_pack(SCHEDP_PRIO, FIXED_PRIO));
			num_child ++;
		}
	}
	assert(num_child);
	hypercall_comp_init_done();

	sl_sched_loop_nonblock();

	PRINTC("ERROR: Should never have reached this point!!!\n");
	assert(0);
}

