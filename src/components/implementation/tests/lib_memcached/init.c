#include <llprint.h>
#include <cos_mc_adapter.h>

void
cos_init(void)
{
	int argc, ret;

	printc("lib memcached init...\n");

	char *argv[] =	{
		"memcached test argv",
	};

	argc = ARRAY_SIZE(argv);

	/* 1. do initialization of memcached */
	ret = cos_mc_init(argc, argv);
	printc("ret: %d\n", ret);
}

int
main(void)
{
	printc("lib memcached test started\n");

	return 0;
}
