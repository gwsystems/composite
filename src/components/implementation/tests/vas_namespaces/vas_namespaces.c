#include <cos_component.h>
#include <llprint.h>

void
cos_init(void)
{
	printc("Hello world (cos init component id = %ld)\n", cos_compid());
}

void
parallel_main(coreid_t cid)
{
	printc("Parallel main component id = %ld\n", cos_compid());
}

