#include <cos_kernel_api.h>
#include <cos_types.h>
#include <ps.h>

#define ITER 1024

void
cos_init(void)
{

	printc("Pong component %ld: cos_init execution\n", cos_compid());

	return;
}

int
main(void)
{
	printc("PONG HELLO WORLD\n");

	return 0;
}
