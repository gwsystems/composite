#ifndef WORK_H
#define WORK_H

#include <cos_types.h>

/* @return: number of actual cycles elapsed */
cycles_t work_cycs(cycles_t ncycs);
/* @return: number of actual usecs elapsed */
microsec_t work_usecs(microsec_t nusecs);


#endif /* WORK_H */
