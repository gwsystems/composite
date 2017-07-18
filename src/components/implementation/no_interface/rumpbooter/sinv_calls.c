#include <cos_kernel_api.h>
#include <cos_types.h>
#include <sinv_calls.h>
#include <shdmem.h>
#include "rumpcalls.h"

int
test_entry(int arg1, int arg2, int arg3, int arg4)
{
        int ret = 0;

        printc("\n*** KERNEL COMPONENT ***\n \tArguments: %d, %d, %d, %d\n", arg1, arg2, arg3, arg4);
        printc("spdid: %d\n", cos_spdid_get());
        printc("*** KERNEL COMPONENT RETURNING ***\n\n");

        return ret;
}

int
test_fs(int arg1, int arg2, int arg3, int arg4)
{
        int ret = 0;

        printc("\n*** KERNEL COMPONENT ***\n \tArguments: %d, %d, %d, %d\n", arg1, arg2, arg3, arg4);
        printc("spdid: %d\n", cos_spdid_get());

        /* FS Test */
        printc("Running paws test: VM%d\n", cos_spdid_get());
        paws_tests();

        printc("*** KERNEL COMPONENT RETURNING ***\n\n");

        return ret;

}

/* TODO Rename this dumb conevention from a function to a system call  */
int
shdmem_allocate(int arg1, int arg2, int arg3, int arg4)
{ return shm_allocate((unsigned int)arg1, arg2, arg3, arg4); }

int
shdmem_deallocate(int arg1, int arg2, int arg3, int arg4)
{ return shm_deallocate(arg1, arg2, arg3, arg4); }

int
shdmem_map(int arg1, int arg2, int arg3, int arg4)
{ return shm_map(arg1, arg2, arg3, arg4); }
