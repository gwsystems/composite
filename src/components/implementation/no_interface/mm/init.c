#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_types.h>

extern void MM_AppMain();
extern void OS_IdleLoop();
extern void do_emulation_setup(spdid_t id);

void cos_init(void)
{
    do_emulation_setup(cos_comp_info.cos_this_spd_id);
    printc("Starting MM main\n");
    MM_AppMain();
    printc("Ending MM main\n");
    while(1) OS_IdleLoop();
}
