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

int
test_shdmem(int shm_id, int arg2, int arg3, int arg4)
{
	int my_id;
	vaddr_t my_page;

	/* Calling from user component into kernel component */
	assert(!cos_spdid_get());

	/* Map in shared page */
	my_id = shmem_map_invoke(shm_id);
	assert(my_id == shm_id);

	/* Get our vaddr for this shm_id */
	my_page = shmem_get_vaddr_invoke(my_id);
	assert(my_page);

	printc("Kernel Component shared mem vaddr: %p\n", (void *)my_page);
	printc("Reading from page: %c\n", *(char *)my_page);
	printc("Writing 'b' to page + 1\n");
	*((char *)my_page + 1) = 'b';

	return 0;
}

/* TODO Rename this dumb conevention from a function to a system call  */
vaddr_t
shdmem_get_vaddr(int arg1, int arg2, int arg3, int arg4)
{ return shm_get_vaddr((unsigned int)arg1, (unsigned int)arg2, arg3, arg4); }

int
shdmem_allocate(int arg1, int arg2, int arg3, int arg4)
{ return shm_allocate((unsigned int)arg1, arg2, arg3, arg4); }

int
shdmem_deallocate(int arg1, int arg2, int arg3, int arg4)
{ return shm_deallocate(arg1, arg2, arg3, arg4); }

int
shdmem_map(int arg1, int arg2, int arg3, int arg4)
{ return shm_map(arg1, arg2, arg3, arg4); }
