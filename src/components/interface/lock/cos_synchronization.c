/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */
#define COS_FMT_PRINT
#include <cos_component.h>
#include <cos_synchronization.h>
#include <print.h>

#define LOG_SZ 256
typedef enum { NIL = 0, PTAKE, TAKE, PRELEASE, RELEASE } log_t;
struct log_entry {
	log_t type;
	u16_t lock_id, thd, owner;
	int contested;
} blahlog[LOG_SZ];
static int location = 0;

static void log_blah(cos_lock_t *l, log_t t)
{
	struct log_entry *le;
	struct log_entry ent = {.type = t, .lock_id = l->lock_id, 
				.thd = cos_get_thd_id(), .owner = l->atom.c.owner, 
				.contested = l->atom.c.contested};
	le = &blahlog[location];
	location = (location+1) % LOG_SZ;
	memcpy(le, &ent, sizeof(struct log_entry));
}

static void log_pretake(cos_lock_t *l)
{
	log_blah(l, PTAKE);
}

static void log_take(cos_lock_t *l)
{
	log_blah(l, TAKE);
}

static void log_prerelease(cos_lock_t *l)
{
	log_blah(l, PRELEASE);
}

static void log_release(cos_lock_t *l)
{
	log_blah(l, RELEASE);
}

static void my_log_print(void)
{
	int i, loc;
	
	printc("lock log for component %d\n", cos_spd_id());
	
	loc = (location+1) % LOG_SZ;
	for (i = loc ; i != location ; i = (i+1) % LOG_SZ) {
		struct log_entry *e = &blahlog[i];
		char *op;

		switch (e->type) {
		case PTAKE:    op = "pretake"; break;
		case TAKE:     op = "take"; break;
		case PRELEASE: op = "prerelease"; break;
		case RELEASE:  op = "release"; break;			
		default:       op = "empty entry"; break;
		}
		printc("%s (%d): own %d, id %d, contest %d.\n", 
		       op, e->thd, e->owner, e->lock_id, e->contested);
	}
	BUG();
}

unsigned int lock_contested(cos_lock_t *l)
{
	return l->atom.c.owner;
}

/* 
 * Return the amount of time that have elapsed since the request was
 * made if we get the lock, _or_ TIMER_EXPIRED if we did not get the
 * lock, but at least microsec microseconds have passed.
 */
int lock_take_timed(cos_lock_t *l, unsigned int microsec)
{
	union cos_lock_atomic_struct result, prev_val;
	unsigned int curr = cos_get_thd_id();
	u16_t owner;
	spdid_t spdid = cos_spd_id();
	unsigned int elapsed_time = 0;
	int lock_id = l->lock_id;

	prev_val.c.owner = prev_val.c.contested = 0;
	result.v = 0;
	/* printc("%d: lt %d (%d, %d) %d\n", */
	/*        curr, l->lock_id, l->atom.c.owner, l->atom.c.contested, spdid); */
	log_pretake(l);
	do {
		int ret;
restart:
		/* Atomically copy the entire 32 bit structure */
		prev_val.v         = l->atom.v;
		owner              = prev_val.c.owner;
		result.c.owner     = curr;
		result.c.contested = 0;
		assert(owner != curr); /* No recursive lock takes allowed */

		/* Contention path: If there is an owner, whom is not
		 * us, go through the motions of blocking on the lock.
		 * This is hopefully the uncommon case. If not, some
		 * structural reconfiguration is probably going to be
		 * needed.  */
		if (unlikely(owner)) {
			if (unlikely(0 == microsec)) return TIMER_EXPIRED;
			if (lock_component_pretake(spdid, lock_id, owner)) {
				/* lock_id not valid */
				return -1;
			}
			/* Must access memory (be volatile) as we want
			 * to detect changes here */
			if (owner != l->atom.c.owner) goto restart;
			/* Mark the lock as contested */
			if (!l->atom.c.contested) {
				result.c.contested = 1;
				result.c.owner     = owner;
				if (!cos_cas((unsigned long*)&l->atom.v, prev_val.v, result.v)) goto restart;
				assert(l->atom.c.contested);
			}
			/* Note if a 1 is returned, there is a
			 * generation mismatch, and we just want to
			 * try and take the lock again anyway */
			ret = lock_component_take(spdid, lock_id, owner, microsec);
			if (ret == -3) my_log_print();
			if (likely(ret >= 0 && ret != TIMER_EXPIRED)) {
				if (microsec != TIMER_EVENT_INF) {
					int diff = microsec - ret;
					microsec = diff > 0 ? diff : 0;
				}
				elapsed_time += ret;
			} else {
				assert(l->atom.c.owner != curr);
				return ret;
			}
			/* try to take the lock again */
			goto restart;
		}
		/* Commit the new lock value, or try again */
		assert(result.v == curr);
	} while (unlikely(!cos_cas((unsigned long *)&l->atom.v, prev_val.v, result.v)));
	log_take(l);
	/* printc("%d:\tlt %d %d\n", curr, l->lock_id, spdid); */
	assert(l->atom.c.owner == curr);

	return elapsed_time;
}

int lock_take(cos_lock_t *l) 
{
	int ret = lock_take_timed(l, TIMER_EVENT_INF);
	/* 
	 * Don't return the return value as the caller doesn't care
	 * about timing for this fn.  If there is an error, however,
	 * pass that down.
	 */
	assert(ret != TIMER_EXPIRED);
	return (0 != ret) ? -1 : 0;
}

int lock_release(cos_lock_t *l) {
	unsigned int curr = cos_get_thd_id();
	union cos_lock_atomic_struct prev_val;
	int lock_id = l->lock_id;

	/* printc("%d: lr %d (%d, %d) %d\n", */
	/*        curr, l->lock_id, l->atom.c.owner, l->atom.c.contested, cos_spd_id()); */
	log_prerelease(l);
	prev_val.c.owner = prev_val.c.contested = 0;
	do {
		assert(sizeof(union cos_lock_atomic_struct) == sizeof(u32_t));
		prev_val.v = l->atom.v; /* local copy of lock */
		/* If we're here, we better own the lock... */
		if (unlikely(prev_val.c.owner != curr)) BUG();
		if (unlikely(prev_val.c.contested)) {
			/* 
			 * This must evaluate to false, as contested
			 * is already set, we are the owner (thus no
			 * other thread should set that),
			 */
			if (!cos_cas((unsigned long*)&l->atom, prev_val.v, 0)) BUG();
			if (lock_component_release(cos_spd_id(), lock_id)) {
				/* Lock doesn't exist */
				return -1;
			}
			return 0;
		}

		/* The loop is necessary as when read, the lock might
		 * not be contested, but by the time we get here,
		 * another thread might have tried to take it. */
	} while (unlikely(!cos_cas((unsigned long *)&l->atom, prev_val.v, 0)));
	log_release(l);
	/* printc("%d:\tlr %d %d\n", curr, l->lock_id, cos_spd_id()); */
	assert(l->atom.c.owner != curr);

	return 0;
}

/* 
 * Cache of lock ids for this component so that we don't have to call
 * the lock component for each lock we create.
 */
u32_t __lid_cache[NCACHED_LOCK_IDS] = {};
int __lid_top;

#ifndef STATIC_ALLOC
cos_lock_t *lock_alloc(void)
{
	cos_lock_t *l = (cos_lock_t*)malloc(sizeof(cos_lock_t));
	if (!l) return 0;

	lock_init(l);
	l->lock_id = lock_id_alloc();
	if (0 == l->lock_id) {
		free(l);
		return 0;
	}

	return l;
}

void lock_free(cos_lock_t *l)
{
	assert(l);
	lock_component_free(cos_spd_id(), l->lock_id);
	free(l);
}
#endif
