#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <initf.h>

#define STEP 7

char b[4096];

void cos_init(void)
{
	int pos, r;

	if (initf_size() > 4095) BUG();
	for (pos = 0 ; 
	     (r = initf_read(pos, &b[pos], STEP)) ; 
	     pos += r) ;
	/* When testing, better be a small text file! */
	b[pos+1] = '\0';
	printc("Init file (read %d, size %d): %s.\n", pos, initf_size(), b);
	
	return;
}

void bin(void)
{
	sched_block(cos_spd_id(), 0);
}
