#include <cos_kernel_api.h>
#include <cos_types.h>
#include <vas_test_call_b.h>
#include <vas_test_call_a.h>
#include <memmgr.h>

void
vas_test_call_b(void) 
{
	int *ptr = (int *)memmgr_heap_page_alloc();
	assert(ptr);

	printc("Component %ld calling server...\n", cos_compid());
	vas_test_call_a();
	printc("Component %ld returning...\n", cos_compid());
	
}

int main(void) {}
