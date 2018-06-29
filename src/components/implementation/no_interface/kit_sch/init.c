#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_types.h>

extern void KIT_SCH_AppMain();
extern void OS_IdleLoop();
extern void do_emulation_setup(spdid_t id);
//extern void posix_rk_init(int);

void cos_init(void)
{
    do_emulation_setup(cos_comp_info.cos_this_spd_id);
//    posix_rk_init(0);

    PRINTC("Starting KIT_SCH main\n");
    KIT_SCH_AppMain();
    PRINTC("Ending KIT_SCH main\n");
    while(1) OS_IdleLoop();
}
