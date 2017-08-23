/***
 * Copyright 2011 by Gabriel Parmer.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2011
 */

#ifndef CSLAB_H
#define CSLAB_H

#ifdef LINUX_TEST
#include <assert.h>
#include <sys/mman.h>
#define CSLAB_ALLOC(sz) mmap(0, sz, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, (size_t)0)
#define CSLAB_FREE(x, sz) munmap(x, CSLAB_MEM_ALLOC_SZ)
#define u16_t unsigned short int
#define u32_t unsigned int
#define unlikely(x) x
#else

#include <cos_component.h>
#include <cos_debug.h>
#endif

#include <string.h>
#include <consts.h>
#include <cos_list.h>
#include <bitmap.h>

/***
 * Simple slab allocator.  Note that "cslab" stands for "Composite
 * Slab", not "CS Lab".
 *
 * To use, your program should include:
 * CSLAB_CREATE(ms, sizeof(my_struct)); //ms is a name chosen by you
 *
 * allocations and deallocations are made:
 * struct my_struct *ms = cslab_alloc_ms();
 * cslab_free_ms(ms);
 *
 * Current assumptions/limitations:
 *
 * - Memory used for each slab is PAGE_SIZE.  Since the slab
 *   descriptor is allocated at the start of this, this can lead to
 *   significant internal fragmentation.  Currently, this slab
 *   implementation should only be used for small allocations.
 */

#define CSLAB_MEM_ALLOC_SZ PAGE_SIZE

#define CSLAB_MIN_ORDER 5 /* minimum slab size = 2^5 = 32 bytes */
#define CSLAB_MIN (1 << CSLAB_MIN_ORDER)
#define CSLAB_MAX_OBJS (PAGE_SIZE / CSLAB_MIN)
#define CSLAB_FIRST_OFF 64 /* first memory allocation is cache-aligned */

#define CSLAB_BITMAP_UNIT 32
#define CSLAB_BITMAP_NUNIT ((unsigned int)(CSLAB_MAX_OBJS / CSLAB_BITMAP_UNIT))
#define CSLAB_BITMAP_SIZE \
	((CSLAB_BITMAP_NUNIT * CSLAB_BITMAP_UNIT != CSLAB_MAX_OBJS) ? CSLAB_BITMAP_NUNIT + 1 : CSLAB_BITMAP_NUNIT)

/* The header for a slab. */
struct cslab {
	u16_t         obj_sz, nfree;
	u32_t         bitmap[CSLAB_BITMAP_SIZE];
	struct cslab *next, *prev; /* freelist next */
} __attribute__((packed));

struct cslab_freelist {
	struct cslab *list;
};

#ifndef CSLAB_ALLOC
#error "You must pound define a CSLAB_ALLOC and CSLAB_FREE"
#endif

static inline struct cslab *
__cslab_lookup(void *buf)
{
	return (struct cslab *)(PAGE_MASK & (u32_t)buf);
}
static inline char *
__cslab_mem_first(struct cslab *s)
{
	return ((char *)s) + CSLAB_FIRST_OFF;
}

static void
__slab_freelist_rem(struct cslab_freelist *fl, struct cslab *s)
{
	assert(s && fl);
	if (fl->list == s) {
		if (EMPTY_LIST(s, next, prev))
			fl->list = NULL;
		else
			fl->list = FIRST_LIST(s, next, prev);
	}
	REM_LIST(s, next, prev);
}

static void
__slab_freelist_add(struct cslab_freelist *fl, struct cslab *s)
{
	assert(s && fl);
	assert(EMPTY_LIST(s, next, prev));
	assert(s != fl->list);
	if (fl->list) ADD_LIST(fl->list, s, next, prev);
	fl->list = s;
	/* TODO: sort based on emptiness -- but is it worth the
	 * complexity of a tree? */
}

static inline void
__cslab_mem_free(void *buf, struct cslab_freelist *fl, int obj_sz, int max_objs)
{
	struct cslab *s = __cslab_lookup(buf);
	int           idx, off;
	assert(s && fl && buf);

	off = (int)((u32_t)buf - (u32_t)__cslab_mem_first(s));
	assert(off >= 0);

	/***
	 * Argh, division!  Our saving grace here is that in the
	 * common case, this function is inlined into the macro
	 * definitions, making both obj_sz and max_objs constants.
	 * Constant propagation will hopefully turn this into shifts
	 * for powers of two, and be as intelligent as possible
	 * otherwise.  Trust your compiler (see the comment before
	 * CSLAB_CREATE).
	 */
	idx = off / obj_sz;
	assert(idx < max_objs);
	assert(!bitmap_check(s->bitmap, idx));
	bitmap_set(s->bitmap, idx);
	s->nfree++;
	if (s->nfree == max_objs) {
		__slab_freelist_rem(fl, s);
		CSLAB_FREE(s, CSLAB_MEM_ALLOC_SZ);
	} else if (s->nfree == 1) {
		assert(EMPTY_LIST(s, next, prev));
		__slab_freelist_add(fl, s);
	}

	return;
}

static void
__cslab_init(struct cslab *s, struct cslab_freelist *fl, int size, int max_objs)
{
	s->obj_sz = size;
	s->nfree  = max_objs;
	memset(s->bitmap, ~0, sizeof(u32_t) * CSLAB_BITMAP_SIZE);
	INIT_LIST(s, next, prev);
	__slab_freelist_add(fl, s);
}

static inline void *
__cslab_mem_alloc(struct cslab_freelist *fl, int obj_sz, int max_objs)
{
	struct cslab *s;
	int           idx;
	u32_t *       bm;
	void *        mem;

	s = fl->list;
	if (unlikely(!s)) {
		s = CSLAB_ALLOC(CSLAB_MEM_ALLOC_SZ);
		if (unlikely(!s)) return NULL;
		__cslab_init(s, fl, obj_sz, max_objs);
	}

	/* find an empty slot */
	bm  = s->bitmap;
	idx = bitmap_one(bm, CSLAB_BITMAP_SIZE);
	assert(idx > -1 && idx < max_objs);
	bitmap_unset(bm, idx);
	mem = __cslab_mem_first(s) + (idx * obj_sz);

	assert(s->nfree);
	s->nfree--;
	/* remove from the freelist */
	if (!s->nfree) __slab_freelist_rem(fl, s);

	return mem;
}

/***
 * This macro is very important.  It creates the functions for
 * allocation and deallocation passing in the freelist directly, and
 * size information for these objects.  This avoids freelist lookups,
 * and relies on the compilers optimizations to generate specialized
 * code for the given sizes -- requires function inlining and constant
 * propagation.  Relying on these optimizations is better than putting
 * all of the code for allocation and deallocation in the macro due to
 * maintenance and readability.
 */
#define CSLAB_CREATE_DATA(name, size)                                                          \
	struct cslab_freelist slab_##name##_freelist = {.list = NULL};                         \
	static const int      slab_##name##_max_objs = ((CSLAB_MEM_ALLOC_SZ - CSLAB_FIRST_OFF) \
                                                   / (round_up_to_pow2(size, WORD_SIZE)))

#define CSLAB_CREATE_FNS(name, size)                                                                     \
	static inline void *cslab_alloc_##name(void)                                                     \
	{                                                                                                \
		return __cslab_mem_alloc(&slab_##name##_freelist, round_up_to_pow2(size, WORD_SIZE),     \
		                         slab_##name##_max_objs);                                        \
	}                                                                                                \
                                                                                                         \
	static inline void cslab_free_##name(void *buf)                                                  \
	{                                                                                                \
		return __cslab_mem_free(buf, &slab_##name##_freelist, round_up_to_pow2(size, WORD_SIZE), \
		                        slab_##name##_max_objs);                                         \
	}

#define CSLAB_CREATE(name, size)       \
	CSLAB_CREATE_DATA(name, size); \
	CSLAB_CREATE_FNS(name, size)

#endif /* CSLAB_H */
