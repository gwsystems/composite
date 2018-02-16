#include <cos_kernel_api.h>
#include <cos_types.h>
#include <cobj_format.h>

extern void SAMPLE_LibInit();
extern void OS_IdleLoop();

void cos_init(void)
{
    printc("Starting lib init\n");
    SAMPLE_LibInit();
    printc("Ending lib init\n");
    while(1) OS_IdleLoop();
}
