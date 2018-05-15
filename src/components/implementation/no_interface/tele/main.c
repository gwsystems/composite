#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_types.h>

#include <osapi.h>

#include "telemetry.h"

extern void do_emulation_setup(spdid_t id);

void cos_init(void)
{
    do_emulation_setup(cos_comp_info.cos_this_spd_id);
    printc("Starting tele main\n");
    TELE_AppMain();
    printc("Ending tele main\n");
    while(1) OS_IdleLoop();
}
