#include <cos_kernel_api.h>
#include <cos_types.h>
#include <sinv_calls.h>
#include "rumpcalls.h"

extern int vmid;

int
test_entry(int arg1, int arg2, int arg3, int arg4)
{
        int ret = 0;

        printc("\n*** KERNEL COMPONENT ***\n \tArguments: %d, %d, %d, %d\n", arg1, arg2, arg3, arg4);
        printc("vmid: %d\n", vmid);
        printc("*** KERNEL COMPONENT RETURNING ***\n\n");

        return ret;
}

int
test_fs(int arg1, int arg2, int arg3, int arg4)
{
        int ret = 0;

        printc("\n*** KERNEL COMPONENT ***\n \tArguments: %d, %d, %d, %d\n", arg1, arg2, arg3, arg4);
        printc("vmid: %d\n", vmid);

        /* FS Test */
        printc("Running paws test: VM%d\n", vmid);
        paws_tests();

        printc("*** KERNEL COMPONENT RETURNING ***\n\n");

        return ret;

}
