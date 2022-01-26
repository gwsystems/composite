#include <shm_bm.h>
#include <limits.h>
#include <consts.h>
#include <string.h>

#define SHM_BM_BITMAP_BLOCK (sizeof (word_t) * CHAR_BIT)
#define SHM_BM_BITS_TO_WORDS(bits) (round_up_to_pow2(bits, SHM_BM_BITMAP_BLOCK) / SHM_BM_BITMAP_BLOCK)
#define SHM_BM_ALLOC_ALIGNMENT (1ul << 22) // sm regions are on 4MB boundries

typedef unsigned char refcnt_t;
typedef unsigned char byte_t;

/* 
 * shm_bm_t is functionally used as a pointer to this struct
   to provide some abstraction 
 * 
 * We can't store pointers to the bitmap, reference counts, or data in
 * the shared memory because the header  will be mapped in different 
 * address spaces; instead offsets from the header are stored.
 */
struct shm_bm_header {
	unsigned long bitm_offset;
	unsigned long refc_offset;
	unsigned long data_offset; 
	unsigned int  nobj;
	size_t        objsz;
};

/* 
 * macros to get a ref to each of the 3 parts of the shared memory 
 * (bitmap, ref counts, data) because we cannot store pointer to 
 * them in the header since this memory is shared across virt
 * address spaces...
 */
#define SHM_BM_BITM(shm) ((word_t *)  (shm + (sizeof (struct shm_bm_header))))
#define SHM_BM_REFC(shm) ((refcnt_t *)(shm + ((struct shm_bm_header *) shm)->refc_offset))
#define SHM_BM_DATA(shm) ((byte_t *)  (shm + ((struct shm_bm_header *) shm)->data_offset))

// ...and to make the code a little cleaner when using the opaque shm_bm_t type
#define SHM_BM_SIZE(shm) (((struct shm_bm_header *) shm)->objsz)
#define SHM_BM_NOBJ(shm) (((struct shm_bm_header *) shm)->nobj)


static inline void
shm_bm_set_contig(word_t *bm, int offset)
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

static int
shm_bm_clz(word_t *bm, void *stop, int *index, int *offset)
{
	int cnt, lz , idx;

	for (idx = 0; bm < stop; idx++) {
		if (*bm != 0) break;
		bm++;
	}

	if (unlikely((void *) bm >= stop)) return -1;	
	cnt = __builtin_clzl(*bm);
	lz  = cnt + idx * SHM_BM_BITMAP_BLOCK;
	*index = idx;
	*offset = SHM_BM_BITMAP_BLOCK - cnt - 1;
	return lz;
}

cbuf_t   
shm_bm_create(shm_bm_t *shm, size_t objsz, size_t allocsz)
{
	struct shm_bm_header *header;
	int                   nobj;
	size_t                bitmap_sz, refcnt_sz, data_sz;
	size_t                alloc;
	cbuf_t                id;

	if (allocsz < objsz) return 0;

	nobj = allocsz / objsz;
	bitmap_sz = SHM_BM_BITS_TO_WORDS(nobj) * sizeof (word_t);
	refcnt_sz = nobj;
	data_sz   = nobj * objsz;

	alloc = sizeof (struct shm_bm_header) + bitmap_sz + refcnt_sz + data_sz;
	if (alloc > SHM_BM_ALLOC_ALIGNMENT) return 0;

	id  = memmgr_shared_page_allocn_aligned(round_up_to_page(alloc)/PAGE_SIZE, SHM_BM_ALLOC_ALIGNMENT, (vaddr_t *) shm);

	if (id == 0) return 0;
	memset((void *) *shm, 0, round_up_to_page(alloc));

	// metadata
	header = (struct shm_bm_header *) *shm;
	header->objsz       = objsz;
	header->nobj        = nobj;
	header->refc_offset = sizeof (struct shm_bm_header) + bitmap_sz; 
	header->data_offset = sizeof (struct shm_bm_header) + bitmap_sz + refcnt_sz; 

	// set nobj bits to free
	shm_bm_set_contig(SHM_BM_BITM(*shm), nobj);
	return id;
}

shm_bm_t 
shm_bm_map(cbuf_t id)
{
	shm_bm_t shm;

	if (memmgr_shared_page_map_aligned(id, SHM_BM_ALLOC_ALIGNMENT, (vaddr_t *) &shm) == 0) return 0;

	return shm;
}

void *
shm_bm_obj_alloc(shm_bm_t shm, shm_bufid_t *id)
{
	int     freebit = 0, idx = 0, off = -1;
	word_t  word_old; 
	word_t *bm;

	// find a free space. could be preempted
	bm = SHM_BM_BITM(shm);
	do {
		freebit = shm_bm_clz(bm, SHM_BM_REFC(shm), &idx, &off);
		if (freebit == -1) return 0;
		word_old = bm[idx];
	} while (!cos_cas(bm + idx, word_old, word_old & ~(1ul << off)));

	cos_faab(SHM_BM_REFC(shm) + freebit, 1);

	*id = (shm_bufid_t) freebit;
	return SHM_BM_DATA(shm) + (freebit * SHM_BM_SIZE(shm));
}

void *   
shm_bm_obj_use(shm_bm_t shm, shm_bufid_t id)
{
	if (unlikely(id >= SHM_BM_NOBJ(shm))) return 0;

	// obj has not been allocated
	if (unlikely((SHM_BM_REFC(shm) + id) == 0)) return 0;

	cos_faab(SHM_BM_REFC(shm) + id, 1);

	return SHM_BM_DATA(shm) + (id * SHM_BM_SIZE(shm));
}

void *   
shm_bm_obj_take(shm_bm_t shm, shm_bufid_t id)
{
	if (unlikely(id >= SHM_BM_NOBJ(shm))) return 0;
	
	// obj has not been allocated
	if (unlikely((SHM_BM_REFC(shm) + id) == 0)) return 0;

	return SHM_BM_DATA(shm) + (id * SHM_BM_SIZE(shm));
}

void
shm_bm_obj_free(void *ptr)
{
	int      obj_idx, bm_idx, bm_offset;
	shm_bm_t shm;
	word_t  *bm;

	// mask out the bits less significant than the allocation alignment
	shm = (shm_bm_t) ((word_t) ptr & ~(SHM_BM_ALLOC_ALIGNMENT - 1));

	obj_idx = ((byte_t *) ptr - SHM_BM_DATA(shm)) / SHM_BM_SIZE(shm);
	if (obj_idx < 0 || obj_idx >= SHM_BM_NOBJ(shm)) return;

	if (cos_faab(SHM_BM_REFC(shm) + obj_idx, -1) > 1)
		return;

	/*droping the last reference, must free obj*/

	bm_idx    = obj_idx / SHM_BM_BITMAP_BLOCK;
	bm_offset = SHM_BM_BITMAP_BLOCK - obj_idx % SHM_BM_BITMAP_BLOCK - 1;
	bm        = SHM_BM_BITM(shm);

	bm[bm_idx] = bm[bm_idx] | (1ul << bm_offset);
}