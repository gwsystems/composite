/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <workload.h>

int workload_cycs_cserialized(unsigned long *hielpased, unsigned long *loelapsed, unsigned long hi_cycs, unsigned long lo_cycs);
int workload_usecs_cserialized(unsigned long *hielpased, unsigned long *loelapsed, unsigned long hi_usecs, unsigned long lo_usecs);

cycles_t
workload_cycs(cycles_t ncycs)
{
	unsigned long hi_in, lo_in, hi_out, lo_out;

	hi_in = (ncycs >> 32);
	lo_in = ((ncycs << 32) >> 32);

	workload_cycs_cserialized(&hi_out, &lo_out, hi_in, lo_in);

	return (((cycles_t) hi_out << 32) | (cycles_t)lo_out);
}

microsec_t
workload_usecs(microsec_t nusecs)
{
	unsigned long hi_in, lo_in, hi_out, lo_out;

	hi_in = (nusecs >> 32);
	lo_in = ((nusecs << 32) >> 32);

	workload_usecs_cserialized(&hi_out, &lo_out, hi_in, lo_in);

	return (((microsec_t) hi_out << 32) | (microsec_t)lo_out);
}
