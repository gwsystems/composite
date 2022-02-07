#ifndef SHM_BM_STATIC_H
#define SHM_BM_STATIC_H

#include <cos_types.h>
#include <memmgr.h>
#include <limits.h>
#include <consts.h>
#include <string.h>

/***
 * `shm_bm` provides an interface for a slab-like allocator of a shared 
 * memory pool to facilitate message passing between components. A shared
 * memory region is instantiated with the ability to allocate objects of
 * a fixed size. 
 * 
 * Internally, objects are stored in a power of 2 amount of space. This
 * is to make resolving pointers from the bitmap faster using bit shits
 * instead of multiplication and division. If an shared memory region is 
 * created to allocate objects that are not of a power of 2 size, the size 
 * of the object is rounded up to the nearest power of 2. 
 * 
 * The shared memory region is aligned in a component's VAS on a power
 * of 2 boundry. This is in order to provide a free API that does not 
 * require a reference to the shared memory header. To resolve the pointer
 * to the shared memory region header from a pointer to an object in the
 * region, the bits of the address that are less significant than the 
 * allocation boundry are masked out. 
 */

typedef unsigned int shm_objid_t;

#define SHM_BM_BITMAP_BLOCK (sizeof (word_t) * CHAR_BIT)
#define SHM_BM_BITS_TO_WORDS(bits) (round_up_to_pow2(bits, SHM_BM_BITMAP_BLOCK) / SHM_BM_BITMAP_BLOCK)

typedef unsigned char refcnt_t;
typedef unsigned char byte_t;

#define SHM_BM_REFC(shm, nobj) ((refcnt_t *) ((byte_t *) header + SHM_BM_BITS_TO_WORDS(nobj) * sizeof (word_t)))
#define SHM_BM_DATA(shm, nobj) ((byte_t *)  (SHM_BM_REFC(header, nobj) + (unsigned int) nobj))


static inline void
__shm_bm_set_contig(word_t *bm, int offset)
{
	int i, n, ind;

	ind = offset / SHM_BM_BITMAP_BLOCK;
	offset %= SHM_BM_BITMAP_BLOCK;

	for (i = 0; i < ind; i++) {
		bm[i] = ~0x0ul;
	}

	n = SHM_BM_BITMAP_BLOCK - offset;
	bm[ind] = ~((1ul << n) - 1); // set most sig n bits of bm[ind]
}

/* Find the first nonzero word in the inputted bitmap */
static inline int
__shm_bm_next_free_word(word_t *bm, unsigned long nwords)
{
	/* the index of the word in the bitmap where the first set bit is found */
	int idx;

	/* iterate through the bitmap to find the first non-zero word */
	for (idx = 0; idx < (int) nwords; idx++) {
		if (*bm != 0) return idx;
		bm++;
	}

	/* all bits set to zero */
	return -1;
}

static inline cbuf_t 
__shm_bm_init(void **header, size_t objsz, unsigned int nobj)
{
	size_t                bitmap_sz, refcnt_sz, data_sz;
	size_t                alloc;
	cbuf_t                id;

	if (nobj <= 0) return 0;

	bitmap_sz = SHM_BM_BITS_TO_WORDS(nobj) * sizeof (word_t);
	refcnt_sz = nobj;
	data_sz   = nobj * objsz;

	alloc = bitmap_sz + refcnt_sz + data_sz;

	id  = memmgr_shared_page_allocn(round_up_to_page(alloc)/PAGE_SIZE, (vaddr_t *) header);
	if (id == 0) return 0;
	memset(*header, 0, round_up_to_page(alloc));

	// set nobj bits to free
	__shm_bm_set_contig((word_t *) *header, nobj);
	return id;
}

static inline int
__shm_bm_map(void **header, cbuf_t id)
{
	int ret;

	ret = memmgr_shared_page_map(id, (vaddr_t *) header);

	if (unlikely(ret == 0)) return 0;

	return 1;
}

static inline void * 
__shm_bm_obj_alloc(void *header, shm_objid_t *objid, size_t objsz, unsigned int nobj)
{
	int      freebit, idx , offset, lz;
	word_t   word; 
	word_t   *bm;

	/* 
	 * Find a free space; could be preempted. Find the next word in
	 * the bitmap with a free bit and get its index in the bitmap, and 
	 * the offset of the free bit in that word. Then try to atomically 
	 * xchg the bitmap with the freebit set to 0
	 */
	bm = (word_t *) header;
	do {
		idx = __shm_bm_next_free_word(bm, SHM_BM_BITS_TO_WORDS(nobj));
		if (idx == -1) return 0;
		word   = bm[idx];
		lz     = __builtin_clzl(word); 
		offset = SHM_BM_BITMAP_BLOCK - lz - 1;
	} while (!cos_cas(bm + idx, word, word & ~(1ul << offset)));

	freebit = lz + (idx * SHM_BM_BITMAP_BLOCK);

	cos_faab(SHM_BM_REFC(header, nobj) + freebit, 1);
	printc("%u\n",*(SHM_BM_REFC(header, nobj) + freebit));

	*objid = (shm_objid_t) freebit;
	return SHM_BM_DATA(header, nobj) + (freebit * objsz);
}

static inline void *   
__shm_bm_obj_use(void *header, shm_objid_t objid, size_t objsz, unsigned int nobj)
{
	if (unlikely(objid >= nobj)) return 0;

	// obj has not been allocated
	
	if (unlikely(*(SHM_BM_REFC(header, nobj) + objid) == 0)) return 0;

	cos_faab(SHM_BM_REFC(header, nobj) + objid, 1);

	return SHM_BM_DATA(header, nobj) + (objid * objsz);
}

static inline void *   
__shm_bm_obj_borrow(void *header, shm_objid_t objid, size_t objsz, unsigned int nobj)
{
	if (unlikely(objid >= nobj)) return 0;

	// obj has not been allocated
	if (unlikely(*(SHM_BM_REFC(header, nobj) + objid) == 0)) return 0;

	return SHM_BM_DATA(header, nobj) + (objid * objsz);
}

static void
__shm_bm_obj_free(void *header, void *ptr, size_t objsz, unsigned int nobj)
{
	unsigned int obj_idx, bm_idx, bm_offset;
	word_t      *bm;

	obj_idx = ((byte_t *) ptr - SHM_BM_DATA(header, nobj)) / objsz;
	if (obj_idx >= nobj) return;

	if (cos_faab(SHM_BM_REFC(shm, nobj) + obj_idx, -1) > 1) { 
		return;
	}

	/* droping the last reference, must set obj to free in bitmap */

	bm         = (word_t *) header;
	bm_idx     = obj_idx / SHM_BM_BITMAP_BLOCK;
	bm_offset  = SHM_BM_BITMAP_BLOCK - obj_idx % SHM_BM_BITMAP_BLOCK - 1;
	bm[bm_idx] = bm[bm_idx] | (1ul << bm_offset);
}

#define __SHM_BM_CREATE_META(name) \
	void *shm_bm_##name_header;

#define __SHM_BM_DEFINE_FCNS(name)                             \
	inline cbuf_t shm_bm_init_##name(void);                    \
	inline int    shm_bm_map_##name(cbuf_t id);                \
	inline void * shm_bm_obj_alloc_##name(shm_objid_t *objid); \
	inline void * shm_bm_obj_use_##name(shm_objid_t objid);    \
	inline void * shm_bm_obj_borrow_##name(shm_objid_t objid); \
	inline void   shm_bm_obj_free_##name(void *ptr);                                                                                         

#define __SHM_BM_CREATE_FCNS(name, objsz, nobjs)                              \
	static inline cbuf_t                                                      \
	shm_bm_init_##name(void)                                                  \
	{                                                                         \
		return __shm_bm_init(&shm_bm_##name_header, objsz, nobjs);            \
	}                                                                         \
	static inline int                                                         \
	shm_bm_map_##name(cbuf_t id)                                              \
	{                                                                         \
		return __shm_bm_map(&shm_bm_##name_header, id);                       \
	}                                                                         \
	static inline void *                                                      \
	shm_bm_obj_alloc_##name(shm_objid_t *objid)                               \
	{                                                                         \
		return __shm_bm_obj_alloc(shm_bm_##name_header, objid, objsz, nobjs); \
	}                                                                          \
	static inline void *                                                       \
		shm_bm_obj_use_##name(shm_objid_t objid)                                      \
	{                                                                             \
		return __shm_bm_obj_use(shm_bm_##name_header, objid, objsz, nobjs);       \
	}                                                                             \
	static inline void *                                                          \
	shm_bm_obj_borrow_##name(shm_objid_t objid)                                   \
	{                                                                             \
		return __shm_bm_obj_borrow(shm_bm_##name_header, objid, objsz, nobjs);    \
	}                                                                             \
	static inline void                                                            \
	shm_bm_obj_free_##name(void *ptr)                                             \
	{                                                                             \
		__shm_bm_obj_free(shm_bm_##name_header, ptr, objsz, nobjs);\
	}                                                                                   

#define SHM_BM_CREATE(name, objsz, nobj)     \
	__SHM_BM_CREATE_META(name)               \
	__SHM_BM_DEFINE_FCNS(name) \
	__SHM_BM_CREATE_FCNS(name, objsz, nobj) 

#endif