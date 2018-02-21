#include <resmgr.h>
#include <sl.h>
#include <res_spec.h>
#include <llboot.h>

#define MAX_CHILD_COMPS 8
#define MAX_CHILD_BITS  64

u32_t cycs_per_usec = 0;
u64_t child_spdbits = 0;
u64_t childsch_bitf = 0;
unsigned int self_init = 0;
extern int schedinit_self(void);

static struct cos_defcompinfo child_defcinfo[MAX_CHILD_COMPS];
static unsigned int num_child;
static struct sl_thd *child_initthd[MAX_CHILD_COMPS] = { NULL };
static struct sl_thd *__initthd = NULL;

#define FIXED_PRIO 1
#define FIXED_PERIOD_MS (10000)
#define FIXED_BUDGET_MS (4000)
#define IS_BIT_SET(v, pos) (v & ((u64_t)1 << pos))

static void
__init_done(void *d)
{
	while (schedinit_self()) sl_thd_block_periodic(0);
	PRINTC("SELF (inc. CHILD) INIT DONE.\n");
	sl_thd_exit();

	assert(0);
}

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

	llboot_comp_childspdids_get(cos_spd_id(), &child_spdbits);
	llboot_comp_childschedspdids_get(cos_spd_id(), &childsch_bitf);
	PRINTC("Child bitmap: %llx, Child-sched bitmap: %llx\n", child_spdbits, childsch_bitf);

	for (i = 0; i < MAX_CHILD_BITS; i++) {
		if (IS_BIT_SET(child_spdbits, i)) {
			PRINTC("Initializing child component %d\n", i + 1);
			cos_defcompinfo_childid_init(&child_defcinfo[num_child], i + 1);

			child_initthd[num_child] = sl_thd_child_initaep_alloc(&child_defcinfo[num_child], IS_BIT_SET(childsch_bitf, i), 1);
			assert(child_initthd[num_child]);

			sl_thd_param_set(child_initthd[num_child], sched_param_pack(SCHEDP_PRIO, FIXED_PRIO));
			sl_thd_param_set(child_initthd[num_child], sched_param_pack(SCHEDP_WINDOW, FIXED_PERIOD_MS));
			sl_thd_param_set(child_initthd[num_child], sched_param_pack(SCHEDP_BUDGET, FIXED_BUDGET_MS));
			num_child ++;
		}
	}

	__initthd = sl_thd_alloc(__init_done, NULL);
	assert(__initthd);
	sl_thd_param_set(__initthd, sched_param_pack(SCHEDP_PRIO, FIXED_PRIO));

	self_init = 1;
	assert(num_child);
	llboot_comp_init_done();

	sl_sched_loop();

	PRINTC("ERROR: Should never have reached this point!!!\n");
	assert(0);
}

