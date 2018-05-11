#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_types.h>

extern void SCH_Lab_AppMain();
extern void OS_IdleLoop();
extern void do_emulation_setup(spdid_t id);

void cos_init(void)
{
    do_emulation_setup(cos_comp_info.cos_this_spd_id);
    printc("Starting SCH_LAB main\n");
    SCH_Lab_AppMain();
    printc("Ending SCH_LAB main\n");
    while(1) OS_IdleLoop();
}
