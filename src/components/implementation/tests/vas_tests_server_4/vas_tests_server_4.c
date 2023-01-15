#include <cos_kernel_api.h>
#include <cos_types.h>
#include <memmgr.h>

void
vas_test_call_b(void) 
{
	printc("Component %ld returning...\n", cos_compid());
	int *ptr = (int *)memmgr_heap_page_alloc();
	assert(ptr);
}

int main(void) {}
