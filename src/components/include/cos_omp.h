/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2019, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#ifndef COS_OMP_H
#define COS_OMP_H

#include <cos_types.h>
#include <omp.h>

#define COS_OMP_MAX_NUM_THREADS (NUM_CPU)

extern void cos_omp_init(void);

#endif /* COS_OMP_H */
