/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef COS_SYNCHONIZATION_H
#define COS_SYNCHONIZATION_H

typedef struct __attribute__((packed)) {
	/* the thread id and generation must be in the same word */
	unsigned short int take_thd, generation;
	unsigned long lock_id;
} cos_lock_t;

#endif
