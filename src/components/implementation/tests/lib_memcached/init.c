#include <llprint.h>
#include <cos_mc_adapter.h>
#include <cos_memcached.h>

void
cos_init(void)
{
	int argc, ret;

	printc("lib memcached init...\n");

	char *argv[] =	{
		"--listen=10.10.2.2",
		"--port=0",// close tcp initialization
		"--udp-port=11211",
		"--threads=64",
		"--protocol=auto",
		"--memory-limit=64",
		"--extended=no_lru_crawler,no_lru_maintainer,no_hashexpand,no_slab_reassign",
	};

	argc = ARRAY_SIZE(argv);

	/* 1. do initialization of memcached */
	ret = cos_mc_init(argc, argv);
	printc("cos_mc_init done, ret: %d\n", ret);

	mc_test();
}

int
main(void)
{
	printc("lib memcached test started\n");

	return 0;
}
