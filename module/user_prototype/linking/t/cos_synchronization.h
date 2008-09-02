/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef COS_SYNCHONIZATION_H
#define COS_SYNCHONIZATION_H

#define STATIC_ALLOC

#include <cos_component.h>
#include <cos_debug.h>
#include <cos_time.h>

#ifndef assert
#define assert(x)
#endif


struct cos_lock_atomic_struct {
	volatile u16_t owner /* thread id || 0 */;
	volatile u8_t rec_cnt, contested; 
} __attribute__((packed));

typedef struct __attribute__((packed)) {
	volatile struct cos_lock_atomic_struct atom;
	u32_t lock_id;
} cos_lock_t;

/* Provided by the synchronization primitive component */
extern int lock_component_take(spdid_t spd, unsigned long lock_id, unsigned short int thd_id, unsigned int microsec);
extern int lock_component_release(spdid_t spd, unsigned long lock_id);
extern int lock_component_pretake(spdid_t spd, unsigned long lock_id, unsigned short int thd);
extern unsigned long lock_component_alloc(spdid_t spdid);
extern void lock_component_free(spdid_t spdid, unsigned long lock_id);

int lock_take(cos_lock_t *t);
int lock_take_timed(cos_lock_t *t, unsigned int microsec);
int lock_release(cos_lock_t *t);

static inline unsigned long lock_id_alloc(void)
{
	return lock_component_alloc(cos_spd_id());
}

static inline int lock_init(cos_lock_t *l)
{
	l->lock_id = 0;
	l->atom.owner = 0;
	l->atom.rec_cnt = l->atom.contested = 0;

	return 0;
}

static inline unsigned long lock_static_init(cos_lock_t *l)
{
	lock_init(l);
	l->lock_id = lock_id_alloc();

	return l->lock_id;
}

#ifndef STATIC_ALLOC
#include <cos_alloc.h>
cos_lock_t *lock_alloc(void);
void lock_free(cos_lock_t *l);
#endif

#endif
