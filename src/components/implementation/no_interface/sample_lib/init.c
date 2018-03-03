#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_types.h>

extern void SAMPLE_LibInit();
extern void OS_IdleLoop();
extern void do_emulation_setup(spdid_t id);

void cos_init(void)
{
    do_emulation_setup(cos_comp_info.cos_this_spd_id);
    printc("Starting lib init\n");
    SAMPLE_LibInit();
    printc("Ending lib init\n");
    while(1) OS_IdleLoop();
}
