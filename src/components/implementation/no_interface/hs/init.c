#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_types.h>

extern void HS_AppMain();
extern void OS_IdleLoop();
extern void do_emulation_setup(spdid_t id);

void cos_init(void)
{
    do_emulation_setup(cos_comp_info.cos_this_spd_id);
    printc("Starting HS main\n");
    HS_AppMain();
    printc("Ending HS main\n");
    while(1) OS_IdleLoop();
}
