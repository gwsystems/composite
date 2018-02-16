#include <cos_kernel_api.h>
#include <cos_types.h>
#include <cobj_format.h>

extern void SAMPLE_AppMain();
extern void OS_IdleLoop();

void cos_init(void)
{
    printc("Starting app main\n");
    SAMPLE_AppMain();
    printc("Ending app main\n");
    while(1) OS_IdleLoop();
}
