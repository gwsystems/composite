#include <stdint.h>

#include <cobj_format.h>
#include <cos_defkernel_api.h>
#include <hypercall.h>
#include <sl.h>
#include <sl_thd.h>

#include "gen/osapi.h"
#include "gen/common_types.h"

#include "cFE_util.h"
#include "ostask.h"

int number_apps = 0;
thdid_t id_overrides[SL_MAX_NUM_THDS] = {0};

int32
OS_ModuleTableInit(void)
{
	return OS_SUCCESS;
}

int32
OS_ModuleLoad(uint32 *module_id, const char *module_name, const char *filename)
{
	return OS_SUCCESS;
}

int32
OS_ModuleUnload(uint32 module_id)
{
	return OS_SUCCESS;
}

int32
get_this_threads_priority()
{
	OS_task_prop_t prop;
	int32          result = OS_TaskGetInfo(sl_thdid(), &prop);

	assert(result == OS_SUCCESS);
	return prop.priority;
}

void
launch_other_component(spdid_t child_id, int is_library)
{
	struct sl_thd *        t;
	struct cos_defcompinfo child_dci;

	assert(child_id > 0);

	cos_defcompinfo_childid_init(&child_dci, child_id);
	t = sl_thd_initaep_alloc(&child_dci, NULL, 0, 0, 0, 0, 0);

	/* We need to override the delegate thread id, so the cFE think it's this thread
	 * Otherwise cFE application id detection is broken
	 */
	id_overrides[sl_thd_thdid(t)] = sl_thdid();

	if (is_library) {
		sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, 1));
		sl_thd_yield(sl_thd_thdid(t));
	} else {
		sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, get_this_threads_priority()));
		number_apps++;
		OS_TaskExit();
	}
}

// Component proxy hack
// To add new component:
// 1) Do all the cFE stuff
// 2) Create a component from the app
// 3) Add an init routine in the component
// 4) Add a proxy here (sensitive to runscript changes)

void
bm_proxy()
{
	launch_other_component(hypercall_comp_id_get("bm"), 0);
}

void
cs_proxy()
{
	launch_other_component(hypercall_comp_id_get("cs"), 0);
}

void
ds_proxy()
{
	launch_other_component(hypercall_comp_id_get("ds"), 0);
}

void
f42_proxy()
{
	launch_other_component(hypercall_comp_id_get("f42"), 0);
}

void
fm_proxy()
{
	launch_other_component(hypercall_comp_id_get("fm"), 0);
}

void
hc_proxy()
{
	launch_other_component(hypercall_comp_id_get("hc"), 0);
}

void
hs_proxy()
{
	launch_other_component(hypercall_comp_id_get("hs"), 0);
}

void
i42_proxy()
{
	launch_other_component(hypercall_comp_id_get("i42"), 0);
}

void
kit_ci_proxy()
{
	launch_other_component(hypercall_comp_id_get("kit_ci"), 0);
}

void
kit_sch_proxy()
{
	launch_other_component(hypercall_comp_id_get("kit_sch"), 0);
}

void
kit_to_proxy()
{
	launch_other_component(hypercall_comp_id_get("kit_to"), 0);
}

void
lc_proxy()
{
	launch_other_component(hypercall_comp_id_get("lc"), 0);
}

void
md_proxy()
{
	launch_other_component(hypercall_comp_id_get("md"), 0);
}

void
mm_proxy()
{
	launch_other_component(hypercall_comp_id_get("mm"), 0);
}

void
sc_proxy()
{
	launch_other_component(hypercall_comp_id_get("sc"), 0);
}

void
sim_proxy()
{
	launch_other_component(hypercall_comp_id_get("sim"), 0);
}

void
tftp_proxy()
{
	launch_other_component(hypercall_comp_id_get("tftp"), 0);
}

int32
cfs_lib_proxy()
{
	/* This is a total fake! CFS Lib doesn't do useful initialization... */
	OS_printf("CFS Lib Initialized.  Version [FAKE INITIALIZTION]");

	return OS_SUCCESS;
}

int32
expat_lib_proxy()
{
	/* TODO: does this work like cfs_lib??... */
	OS_printf("Expat Initialized.  Version [FAKE INITIALIZTION]");

	return OS_SUCCESS;
}

int32
osk_app_fw_proxy()
{
	/* TODO: does this work like cfs_lib??... */
	OS_printf("OSK_APP_FW Initialized.  Version [FAKE INITIALIZTION]");

	return OS_SUCCESS;
}

struct symbol_proxy {
	char *symbol_name;
	void *proxy;
};

struct symbol_proxy proxies[] = {
					    {"BM_AppMain", bm_proxy},
					    {"CS_AppMain", cs_proxy},
					    {"DS_AppMain", ds_proxy},
					    {"F42_AppMain", f42_proxy},
                                            {"FM_AppMain", fm_proxy},
                                            {"HC_AppMain", hc_proxy},
                                            {"HS_AppMain", hs_proxy},
					    {"I42_AppMain", i42_proxy},
					    {"KIT_CI_AppMain", kit_ci_proxy},
					    {"KIT_SCH_AppMain", kit_sch_proxy},
					    {"KIT_TO_AppMain", kit_to_proxy},
                                            {"LC_AppMain", lc_proxy},
					    {"MD_AppMain", md_proxy},
					    {"MM_AppMain", mm_proxy},
					    {"SC_AppMain", sc_proxy},
					    {"SIM_AppMain", sim_proxy},
					    {"TFTP_AppMain", tftp_proxy},
					    {"CFS_LibInit", cfs_lib_proxy},
					    {"EXPAT_Init", expat_lib_proxy},
					    {"OSK_APP_FwInit", osk_app_fw_proxy}
					   };

int32
OS_SymbolLookup(cpuaddr *symbol_address, const char *symbol_name)
{
	int i;
	int sz = sizeof(proxies)/sizeof(proxies[0]);

	for (i = 0; i < sz; i++) {
		if (!strcmp(symbol_name, proxies[i].symbol_name)) {
			*symbol_address = (cpuaddr)proxies[i].proxy;

			return OS_SUCCESS;
		}
	}

	return OS_ERROR;
}

int32
OS_ModuleInfo(uint32 module_id, OS_module_prop_t *module_info)
{
	return OS_ERR_NOT_IMPLEMENTED;
}

int32
OS_SymbolTableDump(const char *filename, uint32 size_limit)
{
	/* Not needed. */
	assert(0);
	return OS_ERR_NOT_IMPLEMENTED;
}
