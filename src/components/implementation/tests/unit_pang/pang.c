#include <cos_component.h>
#include <pang.h>
#include <cos_debug.h>
#include <cos_types.h>

int var = 99;

int
pang_call(void)
{
	assert(cos_inv_token() == 3);
	return var;
}
