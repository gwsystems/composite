#include <llprint.h>
#include "cos_mc_adapter.h"

int
cos_mc_init(int argc, char **argv)
{
	int ret;

	ret = mc_main(argc, argv);

	return ret;
}
