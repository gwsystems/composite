#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>

#include <llprint.h>
#include <sl.h>
#include <sl_lock.h>
#include <sl_thd.h>

extern void rust_init();

void cos_init() {
	printc("Entering rust!\n");
	rust_init();
	printc("Exited rust!\n");
}

void free(void* ptr) {
	assert(0);
}


void* realloc(void *ptr, size_t new_size) {
	assert(0);
	return NULL;
}
