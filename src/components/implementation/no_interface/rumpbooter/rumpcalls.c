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
	rump_bmk_memsize_init();

	crcalls.rump_cos_get_thd_id = cos_get_thd_id;
	crcalls.rump_cos_print 	    = cos_print;
	crcalls.rump_vsnprintf      = vsnprintf;
	crcalls.rump_strcmp         = strcmp;
	crcalls.rump_strncpy        = strncpy;
	crcalls.rump_memcalloc      = cos_memcalloc;
	crcalls.rump_memalloc       = cos_memalloc;
	crcalls.rump_pgalloc        = alloc_page;

	return;
}

extern unsigned long bmk_memsize;
void
rump_bmk_memsize_init(void)
{
	/* (1<<20) == 1 MG */
	bmk_memsize = COS_MEM_USER_PA_SZ - ((1<<20)*2);
}

void *
cos_memcalloc(size_t n, size_t size)
{

	void *rv;
	size_t tot = n * size;

	if (size != 0 && tot / size != n)
		return NULL;

	rv = rump_cos_calloc(n, size);
	return rv;
}

void *
cos_memalloc(size_t nbytes, size_t align)
{

	/* align is not taken into account as of right now */

	void *rv;

	rv = rump_cos_malloc(nbytes);

	return rv;
}
