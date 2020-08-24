/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2020, The George Washington University
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */

#ifndef STATIC_SLAB_H
#define STATIC_SLAB_H

#include <ps.h>
#include <cos_debug.h>

/***
 * The second API similarly provides the logic for allocation and
 * access implementing the following potential state machine
 * transitions:
 *
 * - free -> constructing
 * - constructing -> allocated
 * - constructing -> free
 * - allocated -> free
 *
 * When in the *free* state, an item can be constructed, and its
 * memory should not be accessed. When it is in the *constructed*
 * state, the item's memory should not be accessed as it is still
 * being prepared.
 *
 * This API enables the *state* of the item to be embedded with a
 * pointer. It assumes that pointers have a the least significant bit
 * set to 0.
 *
 * The implementation takes care to interpret a `0` value as *free*,
 * so that it can be applied to items in the BSS without
 * initialization.  Both constructing and allocated are `!0`.
 * Constructing has the least significant bit `!= 0`, while allocated
 * has it `== 0`. The value must include a pointer if allocated (thus
 * resulting in the entire word being `!= 0`), while the pointer can
 * be `0` or not if constructing.
 */
typedef word_t ss_state_t;
/* if we don't need to store a pointer, use this instead of NULL */
#define SS_STATE_NULLVAL (word_t)(~1)
/*
 * This is used for documentation of what the pointer type is even
 * though it is encapsulated in the ss_state_t
 */
#define SS_STATE_T(type) ss_state_t

/* State encodings in the least significant bit. */
#define SS_STATE_FREE	0	/* free */
#define SS_STATE_CONS	1	/* constructing */
#define SS_STATE_ALLOC	0	/* allocated, co-located with value */

/**
 * Move the memory into the constructing state. Can fail due to
 * concurrent/parallel allocation of the same memory.
 *
 * - @state - the state + ptr or other value
 * - @return - `0` = success, `1` = failure
 */
static inline int
ss_state_alloc(ss_state_t *state) /* free->constructing */
{
	if (*state != SS_STATE_FREE ||
	    !ps_cas(state, SS_STATE_FREE, SS_STATE_CONS)) return 1;

	return 0;
}

/**
 * Activate constructing memory. CAS is not required here as we should
 * have exclusive access.
 */
static inline void
ss_state_activate(ss_state_t *state) /* constructing->allocated */
{
	assert((*state & 1) == SS_STATE_CONS);
	*state &= ~SS_STATE_CONS;
	ps_mem_fence();	/* make sure that the previous writes are committed */
	/* Note that this assumes that the pointer has been set */
	assert(*state != 0);
}

static inline void
ss_state_free(ss_state_t *state) /* (constructing|allocated)->free */
{
	assert(*state != 0);
	*state = SS_STATE_FREE;
}

static inline int
ss_state_is_allocated(ss_state_t state)
{
	return ((state & ~1) != 0) && ((state & 1) == SS_STATE_ALLOC);
}

static inline int
ss_state_is_free(ss_state_t state)
{
	return state == 0;
}

/**
 * This should only be used on allocated memory.
 */
static inline word_t
ss_state_val_get(ss_state_t state)
{
	assert(ss_state_is_allocated(state));

	return (word_t)(state & ~1);
}

/**
 * This can be used on constructing or allocated memory, and it does
 * *not* change the allocation state.
 */
static inline void
ss_state_val_set(ss_state_t *state, word_t val)
{
	assert(ss_state_is_allocated(*state) && val);
	*state = val | (*state & 1);
}

/**
 * Update the state atomically. This assumes that the state is already
 * allocated, thus is using the state as a synchronization mechanism
 * that maintain atomicity WRT the liveness of the state *and* the
 * value associated with the state. Can be used for locking, or for
 * reference counting.
 */
static inline int
ss_state_update(ss_state_t *state, word_t val, word_t prev_val)
{
	assert(ss_state_is_allocated(*state) && val);

	return !ps_cas(state, prev_val, val);
}

/**
 * Store a pointer in the state *and* activate the memory.
 */
static inline void
ss_state_activate_with(ss_state_t *state, word_t val)
{
	assert(val != 0);
	*state = val | (*state & 1);
	ss_state_activate(state);
}

/**
 * If the state is going to be used as a refcnt, this initializes it
 * as such.
 */
static inline void
ss_state_activate_refcnt(ss_state_t *state)
{
	/* Make sure that the value is non-zero, and the LSB is set */
	ss_state_activate_with(state, 2);
}

/**
 * Atomically add `amnt` to the state. If you want to add to the
 * refcnt, pass in `1`; if you want to decrement the refcnt, pass in
 * `-1`.
 *
 * - @state - the state variable
 * - @amnt - the non-zero value to update the state with
 * - @return - the previous value in the state
 */
static inline long
ss_state_refcnt_add(ss_state_t *state, long amnt)
{
	long prev;

	assert(ss_state_is_allocated(*state) && amnt != 0);
	if (amnt < 0) amnt = -((-amnt) << 2);
	else          amnt = amnt << 2;
	prev = ps_faa(state, amnt);
	if (prev < 0) prev = -((-prev) >> 2);
	else          prev = prev >> 2;

	return prev;
}

/***
 * A memory allocator for statically-allocated collections of
 * objects. Similar to an API, and a baby version of parsec
 * namespaces. Objects are tracked as state machines of object
 * initialization, use, and freeing. This structure supports *indexed*
 * lookups, thus providing a facility to map a token/descriptor to an
 * object. This is mainly useful in system code that shares these
 * descriptors between protection domains (where pointers are
 * invalid). The implementation tracks the allocation state of an
 * object, *and* provides some rudimentary synchronization for indexed
 * lookups.
 *
 * The API is generated by the `SS_STATIC_ALLOC` macro, specialized to
 * the specific type. This enables the alignment constraints of the
 * underlying type to be factored into the global allocation, and
 * avoids magic math based on the size of the object type. It also
 * mimics the typical slab allocator interface.
 *
 * *The API*:
 * - `SS_STATIC_SLAB(name, type, max_num_objects)` - Create the global
 *   memory for the objects to be able to allocate `max_num_objects`
 *   number of objects, and the following functions.
 * - `type *ss_name_alloc()` - Allocate a new object of the
 *   specified type. This is not fast. It is linear in max_num_objects.
 * - `void ss_name_free(type *obj)` - Free an allocated object,
 *   `obj`.
 * - `type *ss_name_alloc_at_index(unsigned int idx)` -
 *   Allocate an object at a descriptor `idx`.
 * - `void ss_name_activate(type *obj)` - Made an object
 *   accessible through indexed lookups.
 * - `type *ss_name_get(unsigned int idx) - Lookup an object via
 *   descriptor `idx` and return it if it is active.
 * - `unsigned int ss_name_index(type *obj)` - return the index
 *   of the object.
 *
 * *Example*:
 * ```c
 * SS_STATIC_SLAB(comp, struct crt_comp, MAX_NUM_COMPS);
 *
 * struct crt_comp *
 * comp_alloc(compid_t *cid)
 * {
 * 	struct crt_comp *c = ss_comp_alloc();
 *
 * 	if (!c) return NULL;
 * 	*cid = ss_comp_index(c);
 * 	// initialize the component
 * 	ss_comp_activate(c);
 *
 * 	return c;
 * }
 *
 * int
 * comp_free(compid_t id)
 * {
 * 	struct crt_comp *c ss_comp_get(id);
 *
 * 	if (!c) return -ENVAL;
 * 	ss_comp_free(c);
 *
 *	return 0;
 * }
 * ```
 *
 * *Synchronization*: It is important to further discuss the
 * interaction between `activate` and `get`. An object transitions
 * from `free` to `constructing` to denote that it's memory is in use,
 * but it is *not* yet accessible for indexed lookups. At this point,
 * an indexed lookup will return false. After the object is
 * initialized properly (denoted by `*_activate`), it transitions into
 * `allocated` and accessible for indexed lookups.
 *
 * This is all of the synchronization necessary to track allocations,
 * and for objects that are read-only, or can handle modification in
 * place (e.g. by wrapping them with a lock, or using atomic
 * instructions).
 *
 * *Allocator metadata*: The implementation uses a separation between
 * meta-data (allocation states) from the allocated memory. It
 * allocates a word to each object's meta-data, thus giving a wastage
 * fraction of sizeof(word_t)/(sizeof(type) + sizeof(word_t)). We
 * don't use bitfields for the meta-data as we don't want potential
 * cas contention *between* different objects. This would require
 * unbounded `cas` loops that are unacceptable. We pay for this with
 * some memory wastage.
 *
 * *Id API*: The API has `*_offset` APIs that are internal, and `*_id`
 * *APIs. The id* APIs view the static allocation space as
 *
 * 1. Offset into the id namespace by `id_offset`. For example, an
 *    `id_offset = 1`, means that `id = 1` is the 0th item in the
 *    static allocation array.
 * 2. The ids are in the range `[id_offset, id_offset + max_num)`.
 *
 * *Guarantees*:
 *
 * - All allocation is through the static array. No dynamic allocation.
 * - Constant-time (array indexed) `get`.
 * - Constant-time `alloc_at_index`.
 * - Objects are laid out contiguously. Alignment constraints should
 *   be embedded into your object/struct type, and they are honored.
 * - Statically allocated memory is BSS-allocated.
 * - Atomic instruction-guaranteed failure of `get` if an object has
 *   not been `activate`d.
 *
 * *Assumptions*:
 *
 * - This macro must be used in the same compilation object (`.o`) in
 *   which it is used. It does *not* generate function prototypes to
 *   be exported, and all data is `static`. Wrap these if you want to
 *   export them. The main macro should *never* be used in a header.
 * - After allocation and preparation of the object, you'll call
 *   `activated` if you're using the indexed allocation mechanism.
 * - The implementation uses `cas` to prevent races on allocation.
 */

/*
 * Create the functions for the main logic. Assumes that each function
 * takes the heap as an argument. This means that different heaps can
 * be used with the same set of functions. However, each heap must be
 * exactly the same size (defined by the `#define` at the bottom of
 * this macro).
 */
#define SS_STATIC_SLAB_FNS(name, obj_type, max_num, id_offset)		\
	struct ss_##name##_heap {					\
		ss_state_t states[max_num];				\
		obj_type   objs[max_num];				\
	};								\
	static obj_type *	/* Not part of the public API */	\
	__ss_##name##_alloc_at_index(struct ss_##name##_heap *heap, unsigned int idx) \
	{								\
		if (idx >= max_num) return NULL;			\
		if (ss_state_alloc(&heap->states[idx])) return NULL;	\
		memset(&heap->objs[idx], 0, sizeof(obj_type));		\
									\
		return &heap->objs[idx];				\
	}								\
	static obj_type *						\
	ss_##name##_alloc_at_id(struct ss_##name##_heap *heap, unsigned int id) \
	{								\
		if (id < id_offset) return NULL;			\
									\
		return __ss_##name##_alloc_at_index(heap, id - id_offset); \
	}								\
	static obj_type *						\
	ss_##name##_alloc(struct ss_##name##_heap *heap)		\
	{								\
		unsigned int i;						\
		obj_type *c = NULL;					\
									\
		for (i = 0; i < max_num; i++) {				\
			c = __ss_##name##_alloc_at_index(heap, i);	\
			if (c) return c;				\
		}							\
									\
		return NULL;						\
	}								\
	static unsigned int /* not part of the public API */		\
	__ss_##name##_index(struct ss_##name##_heap *heap, obj_type *o)	\
	{								\
		assert(o >= heap->objs && o <= &heap->objs[max_num - 1]); \
									\
		return o - heap->objs;					\
	}								\
	static unsigned int						\
	ss_##name##_id(struct ss_##name##_heap *heap, obj_type *o)	\
	{								\
		return __ss_##name##_index(heap, o) + id_offset;	\
	}								\
	static void							\
	ss_##name##_activate(struct ss_##name##_heap *heap, obj_type *o) \
	{								\
		unsigned int idx = __ss_##name##_index(heap, o);	\
									\
		ss_state_activate_with(&heap->states[idx], SS_STATE_NULLVAL); \
	}								\
	static void							\
	ss_##name##_free(struct ss_##name##_heap *heap, obj_type *o)	\
	{								\
		unsigned int idx = __ss_##name##_index(heap, o);	\
									\
		ss_state_free(&heap->states[idx]);			\
	}								\
	static obj_type *						\
	ss_##name##_get(struct ss_##name##_heap *heap, unsigned int id)	\
	{								\
		unsigned int idx;					\
									\
		if (id < id_offset) return NULL;			\
		idx = id - id_offset;					\
		if (idx >= max_num) return NULL;			\
		if (!ss_state_is_allocated(heap->states[idx])) return NULL; \
									\
		return &heap->objs[idx];				\
	}								\
	int								\
	ss_##name##_is_allocated(struct ss_##name##_heap *heap, obj_type *o) \
	{								\
		unsigned int idx = __ss_##name##_index(heap, o);	\
									\
		return !ss_state_is_allocated(heap->states[idx]);	\
	}

/*
 * This creates the static slab allocation functions associated with
 * *globally allocated* memory. It also enables an *id* offset into
 * the array. This can enable id 0 to be an error, rather than an
 * entry in the data-structure.
 */
#define SS_STATIC_SLAB_GLOBAL_ID(name, type, max_num, off)		\
	SS_STATIC_SLAB_FNS(name##_intern, type, max_num, off)		\
	struct ss_##name##_heap { struct ss_##name##_intern_heap h; };	\
	static struct ss_##name##_heap __ss_##name##_heap;		\
	static type *							\
	ss_##name##_alloc_at_id(unsigned int id)			\
	{ return ss_##name##_intern_alloc_at_id(&__ss_##name##_heap.h, id); } \
	static type *							\
	ss_##name##_alloc(void)						\
	{ return ss_##name##_intern_alloc(&__ss_##name##_heap.h); }	\
	static unsigned int						\
	ss_##name##_id(type *o)						\
	{ return ss_##name##_intern_id(&__ss_##name##_heap.h, o); }	\
	static void							\
	ss_##name##_activate(type *o)					\
	{ ss_##name##_intern_activate(&__ss_##name##_heap.h, o); }	\
	static void							\
	ss_##name##_free(type *o)					\
	{ ss_##name##_intern_free(&__ss_##name##_heap.h, o); }		\
	static type *							\
	ss_##name##_get(unsigned int id)				\
	{ return ss_##name##_intern_get(&__ss_##name##_heap.h, id); }	\
	int								\
	ss_##name##_is_allocated(type *o)				\
	{ return ss_##name##_intern_is_allocated(&__ss_##name##_heap.h, o); }

/*
 * Default static allocation API that uses a global heap, and ids
 * starting at value 1 (thus id "0" is an error value).
 */
#define SS_STATIC_SLAB(name, type, max_num) SS_STATIC_SLAB_GLOBAL_ID(name, type, max_num, 1)

#endif	/* STATIC_SLAB_H */
