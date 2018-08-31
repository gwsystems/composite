#ifndef WORKGEN_H
#define WORKGEN_H

#include <cos_types.h>

/* @return: number of actual cycles elapsed */
cycles_t workgen_cycs(cycles_t ncycs);
/* @return: number of actual usecs elapsed */
microsec_t workgen_usecs(microsec_t nusecs);


#endif /* WORKGEN_H */
