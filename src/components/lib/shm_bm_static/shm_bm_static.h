#ifndef SHM_BM_STATIC_H
#define SHM_BM_STATIC_H

#include <cos_types.h>
#include <memmgr.h>
#include <limits.h>
#include <consts.h>
#include <string.h>

/***
 */

#define SHM_BM_BITMAP_BLOCK (sizeof (word_t) * CHAR_BIT)
#define SHM_BM_ALIGN (1 << 22)
#define SHM_BM_BITS_TO_WORDS(bits) (round_up_to_pow2(bits, SHM_BM_BITMAP_BLOCK) / SHM_BM_BITMAP_BLOCK)
#define SHM_BM_SERVICE_TABLE_SIZE MAX_NUM_COMPS

typedef void *        shm_buf_t;
typedef int           shm_reqtok_t;
typedef unsigned int  shm_objid_t;

struct shm_service_tbl_ent {
	invtoken_t authtok;
	void *     shmptr;
};

typedef unsigned long word_t;
typedef unsigned char refcnt_t;
typedef unsigned char byte_t;

#define SHM_BM_REFC(shm, nobj) ((refcnt_t *) ((byte_t *) shm + SHM_BM_BITS_TO_WORDS(nobj) * sizeof (word_t)))
#define SHM_BM_DATA(shm, nobj) ((byte_t *)  (SHM_BM_REFC(shm, nobj) + (unsigned int) nobj))

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
__shm_bm_clt_create(shm_buf_t *shm, size_t objsz, unsigned int nobj)
{
	size_t                bitmap_sz, refcnt_sz, data_sz;
	size_t                alloc;
	cbuf_t                id;

	if (nobj <= 0) return 0;

	bitmap_sz = SHM_BM_BITS_TO_WORDS(nobj) * sizeof (word_t);
	refcnt_sz = nobj;
	data_sz   = nobj * objsz;

	alloc = bitmap_sz + refcnt_sz + data_sz;

	id  = memmgr_shared_page_allocn_aligned(round_up_to_page(alloc)/PAGE_SIZE, SHM_BM_ALIGN, (vaddr_t *) shm);
	if (id == 0) return 0;
	memset(*shm, 0, round_up_to_page(alloc));

	// set nobj bits to free
	__shm_bm_set_contig((word_t *) *shm, nobj);
	return id;
}

static inline void * 
__shm_bm_clt_alloc(shm_buf_t shm, shm_objid_t *objid, size_t objsz, unsigned int nobj)
{
	int     freebit, idx , offset, lz;
	word_t  word; 
	word_t *bm;

	/* 
	 * Find a free space; could be preempted. Find the next word in
	 * the bitmap with a free bit and get its index in the bitmap, and 
	 * the offset of the free bit in that word. Then try to atomically 
	 * xchg the bitmap with the freebit set to 0
	 */
	bm = (word_t *) shm;
	do {
		idx = __shm_bm_next_free_word(bm, SHM_BM_BITS_TO_WORDS(nobj));
		if (idx == -1) return 0;
		word   = bm[idx];
		lz     = __builtin_clzl(word); 
		offset = SHM_BM_BITMAP_BLOCK - lz - 1;
	} while (!cos_cas(bm + idx, word, word & ~(1ul << offset)));

	freebit = lz + (idx * SHM_BM_BITMAP_BLOCK);

	cos_faab(SHM_BM_REFC(shm, nobj) + freebit, 1);

	*objid = (shm_objid_t) freebit;
	return SHM_BM_DATA(shm, nobj) + (freebit * objsz);
}

static inline shm_reqtok_t
__shm_bm_srv_map(struct shm_service_tbl_ent *srv_tbl, shm_reqtok_t *nxt_reqtok, cbuf_t shm_id)
{
	struct shm_service_tbl_ent *entry;

	if (unlikely(*nxt_reqtok >= SHM_BM_SERVICE_TABLE_SIZE)) return -1;
	entry = &srv_tbl[*nxt_reqtok];

	if (unlikely(memmgr_shared_page_map_aligned(shm_id, SHM_BM_ALIGN, (vaddr_t *) &entry->shmptr)) == 0) return -1;
	entry->authtok = cos_inv_token();
	return (*nxt_reqtok)++;
}

static inline struct shm_service_tbl_ent *
__shm_bm_srv_auth(struct shm_service_tbl_ent *srv_tbl, shm_reqtok_t reqtok)
{
	struct shm_service_tbl_ent *entry;

	if (unlikely(reqtok >= SHM_BM_SERVICE_TABLE_SIZE || reqtok < 0)) return 0;

	entry = &srv_tbl[reqtok];
	if (unlikely(entry->authtok != cos_inv_token())) return 0;

	return entry;
}

static inline void *   
__shm_bm_srv_use(struct shm_service_tbl_ent *srv_tbl, shm_reqtok_t reqtok, shm_objid_t objid, size_t objsz, unsigned int nobj)
{
	struct shm_service_tbl_ent *entry;
	void                       *shmptr;

	if (unlikely(objid >= nobj)) return 0;
	if (unlikely((entry = __shm_bm_srv_auth(srv_tbl, reqtok)) == 0)) return 0;

	shmptr = entry->shmptr;
	// obj has not been allocated
	if (unlikely(*(SHM_BM_REFC(shmptr, nobj) + objid) == 0)) return 0;

	cos_faab(SHM_BM_REFC(shmptr, nobj) + objid, 1);

	return SHM_BM_DATA(shmptr, nobj) + (objid * objsz);
}

static inline void *   
__shm_bm_srv_borrow(struct shm_service_tbl_ent *srv_tbl, shm_reqtok_t reqtok, shm_objid_t objid, size_t objsz, unsigned int nobj)
{
	struct shm_service_tbl_ent *entry;
	void                       *shmptr;

	if (unlikely(objid >= nobj)) return 0;
	if (unlikely((entry = __shm_bm_srv_auth(srv_tbl, reqtok)) == 0)) return 0;

	shmptr = entry->shmptr;
	// obj has not been allocated
	if (unlikely(*(SHM_BM_REFC(shmptr, nobj) + objid) == 0)) return 0;

	return SHM_BM_DATA(shmptr, nobj) + (objid * objsz);
}

static void
__shm_bm_ptr_free(void *ptr, size_t objsz, unsigned int nobj)
{
	void        *shm;
	unsigned int obj_idx, bm_idx, bm_offset;
	word_t      *bm;

	shm = (void *) ((word_t) ptr & ~(SHM_BM_ALIGN - 1));
	obj_idx = ((byte_t *) ptr - SHM_BM_DATA(shm, nobj)) / objsz;
	if (obj_idx >= nobj) return;
	
	if (cos_faab(SHM_BM_REFC(shm, nobj) + obj_idx, -1) > 1) { 
		return;
	}
	/* droping the last reference, must set obj to free in bitmap */
	bm         = (word_t *) shm;
	bm_idx     = obj_idx / SHM_BM_BITMAP_BLOCK;
	bm_offset  = SHM_BM_BITMAP_BLOCK - obj_idx % SHM_BM_BITMAP_BLOCK - 1;
	bm[bm_idx] = bm[bm_idx] | (1ul << bm_offset);
}

#define __SHM_BM_DEFINE_FCNS(name)														\
	static inline cbuf_t       shm_bm_clt_create_##name(shm_buf_t *shm);							\
	static inline void *       shm_bm_clt_alloc_##name(shm_buf_t shm, shm_objid_t *objid); 		\
	static inline shm_reqtok_t shm_bm_srv_map_##name(cbuf_t id);								\
	static inline void *       shm_bm_srv_use_##name(shm_reqtok_t reqtok, shm_objid_t objid);		\
	static inline void *       shm_bm_srv_borrow_##name(shm_reqtok_t reqtok, shm_objid_t objid);	\
	static inline void         shm_bm_ptr_free_##name(void *ptr);                                                                                         

#define __SHM_BM_CREATE_FCNS(name, objsz, nobjs)									\
	static inline cbuf_t															\
	shm_bm_clt_create_##name(shm_buf_t *shm)											\
	{																				\
		return __shm_bm_clt_create(shm, objsz, nobjs);								\
	}																				\
	static inline void *															\
	shm_bm_clt_alloc_##name(shm_buf_t shm, shm_objid_t *objid)						\
	{																				\
		return __shm_bm_clt_alloc(shm, objid, objsz, nobjs);						\
	}																				\
	static inline shm_reqtok_t																\
	shm_bm_srv_map_##name(cbuf_t id)												\
	{																				\
		extern struct shm_service_tbl_ent service_tbl_##name[SHM_BM_SERVICE_TABLE_SIZE];;						\
		extern shm_reqtok_t                nxt_reqtok_##name;						\
		return __shm_bm_srv_map(service_tbl_##name, &nxt_reqtok_##name, id);			\
	}																				\
	static inline void *															\
	shm_bm_srv_use_##name(shm_reqtok_t reqtok, shm_objid_t objid)						\
	{																				\
		extern struct shm_service_tbl_ent service_tbl_##name[SHM_BM_SERVICE_TABLE_SIZE];;						\
		return __shm_bm_srv_use(service_tbl_##name, reqtok, objid, objsz, nobjs);	\
	}																				\
	static inline void *															\
	shm_bm_srv_borrow_##name(shm_reqtok_t reqtok, shm_objid_t objid)					\
	{																				\
		extern struct shm_service_tbl_ent service_tbl_##name[SHM_BM_SERVICE_TABLE_SIZE];;						\
		return __shm_bm_srv_use(service_tbl_##name, reqtok, objid, objsz, nobjs);	\
	}																				\
	static inline void																\
	shm_bm_ptr_free_##name(void *ptr)												\
	{																				\
		__shm_bm_ptr_free(ptr, objsz, nobjs);										\
	}

#define SHM_BM_INTERFACE_CREATE(name, objsz, nobj)	\
	__SHM_BM_DEFINE_FCNS(name)						\
	__SHM_BM_CREATE_FCNS(name, objsz, nobj) 

#define SHM_BM_SERVER_INIT(name) 						\
	shm_reqtok_t                nxt_reqtok_##name = 0;	\
	struct shm_service_tbl_ent service_tbl_##name[SHM_BM_SERVICE_TABLE_SIZE];

#endif