/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <cos_types.h>
#include <llprint.h>
#include <srv_dummy.h>

int
srv_dummy_hello(int a, int b, int c)
{
	int ret = a + b + c;

	/* PRINTC("%s: a=%d b=%d c=%d, ret=%d", __func__, a, b, c, ret); */

	return ret;
}

int
srv_dummy_goodbye(int *r2, int *r3, int a, int b, int c)
{
	int r1 = c;

	*r2 = b;
	*r3 = a;
	/* PRINTC("%s: a=%d b=%d c=%d, r1=%d, r2=%d, r3=%d", __func__, a, b, c, r1, *r2, *r3); */

	return r1;
}

int
srv_dummy_thdinit(thdid_t tid)
{
	return 0;
}
