#ifndef COS_SYNCHONIZATION_H
#define COS_SYNCHONIZATION_H

typedef struct {
	/* the thread id and generation must be in the same word */
	unsigned short int take_thd, generation;
	int lock_id;
} cos_lock_t __attribute__((packed));

#endif
