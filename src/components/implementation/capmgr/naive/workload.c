#include <workload.h>
#include <sl.h>
#include "spinlib.h"

int
workload_cycs_cserialized(unsigned long *hielapsed, unsigned long *loelapsed, unsigned long hi_cycs, unsigned long lo_cycs)
{
	cycles_t st, end, elapsed, cycs_input = (((cycles_t)hi_cycs << 32) | (cycles_t)lo_cycs);

	rdtscll(st);
	spinlib_cycles(cycs_input);
	rdtscll(end);
	elapsed = end - st;

	*hielapsed = (elapsed >> 32);
	*loelapsed = ((elapsed << 32) >> 32);

	return 0;
}

int
workload_usecs_cserialized(unsigned long *hielapsed, unsigned long *loelapsed, unsigned long hi_us, unsigned long lo_us)
{
	cycles_t st, end;
	microsec_t elapsed, usecs_input = (((microsec_t)hi_us << 32) | (microsec_t)lo_us);

	rdtscll(st);
	spinlib_usecs(usecs_input);
	rdtscll(end);
	/* perhaps use spinlib to return the elapsed or use sl.. */
	elapsed = sl_cyc2usec(end - st);

	*hielapsed = (elapsed >> 32);
	*loelapsed = ((elapsed << 32) >> 32);

	return 0;

}
