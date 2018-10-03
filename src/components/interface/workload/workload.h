#ifndef WORKLOAD_H
#define WORKLOAD_H

#include <cos_types.h>

/* @return: number of actual cycles elapsed */
cycles_t workload_cycs(cycles_t ncycs);
/* @return: number of actual usecs elapsed */
microsec_t workload_usecs(microsec_t nusecs);


#endif /* WORKLOAD_H */
