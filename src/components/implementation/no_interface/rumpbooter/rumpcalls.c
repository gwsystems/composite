#include <stdio.h>
#include <string.h>
#include <cos_component.h>
#include <cos_alloc.h>

#include "rumpcalls.h"
#include "rump_cos_alloc.h"

extern struct cos_rumpcalls crcalls;

/* Mapping the functions from rumpkernel to composite */

void
cos2rump_setup(void)
{

	crcalls.rump_cos_get_thd_id = cos_get_thd_id;
	crcalls.rump_cos_print 	    = cos_print;
	crcalls.rump_vsnprintf      = vsnprintf;
	crcalls.rump_strcmp         = strcmp;
	crcalls.rump_strncpy        = strncpy;
	crcalls.rump_memcalloc      = cos_memcalloc;
	crcalls.rump_memalloc       = cos_memalloc;

	return;
}

void *
cos_memcalloc(size_t n, size_t size)
{

	void *rv;
	size_t tot = n * size;

	if (size != 0 && tot / size != n)
		return NULL;

	//RENAME ME.
	rv = rump_cos_calloc(n, size);
	return rv;
}

void *
cos_memalloc(size_t nbytes, size_t align)
{

	/* align is not taken into account as of right now */

	void *rv;

	//RENAME ME
	rv = rump_cos_malloc(nbytes); // Malloc call goes to rumpkernel malloc and starts infinite loop.

	return rv;
}
