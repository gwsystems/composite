#include <cos_kernel_api.h>
#include <cos_types.h>
#include <cobj_format.h>

extern void SCH_Lab_AppMain();
extern void OS_IdleLoop();

void cos_init(void)
{
    printc("Starting sch main\n");
    SCH_Lab_AppMain();
    printc("Ending sch main\n");
    while(1) OS_IdleLoop();
}
