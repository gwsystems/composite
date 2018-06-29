#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_types.h>

extern void HC_AppMain();
extern void OS_IdleLoop();
extern void do_emulation_setup(spdid_t id);

void cos_init(void)
{
    do_emulation_setup(cos_comp_info.cos_this_spd_id);
    PRINTC("Starting HC main\n");
    HC_AppMain();
    PRINTC("Ending HC main\n");
    while(1) OS_IdleLoop();
}
