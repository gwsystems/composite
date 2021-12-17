#include <shm_bm.h>
#include <limits.h>
#include <consts.h>
#include <string.h>

#define SHM_BM_BITMAP_BLOCK (sizeof (word_t) * CHAR_BIT)
#define SHM_BM_BITS_TO_WORDS(bits) (round_up_to_pow2(bits, SHM_BM_BITMAP_BLOCK) / SHM_BM_BITMAP_BLOCK)
#define SHM_BM_ALLOC_BOUNDRY 1ul << 22 // sm regions are on 4MB boundries

typedef unsigned char refcnt_t;
typedef unsigned char byte_t;

/* 
 * This header is abstracted from the user for ease-of-use. shm_bm_t is 
 * functionally used as a pointer to this struct. 
 * 
 * We can't store pointers to the bitmap, reference counts, or data in
 * the shared memory because this is a header for memory that will be 
 * mapped in different address spaces; instead offsets from the header
 * are stored.
 */
struct shm_bm_header {
    unsigned long bitm_offset;
    unsigned long refc_offset;
    unsigned long data_offset; 
    unsigned int  nobj;
    size_t        objsz;
    refcnt_t      refcnt;
};

/* 
 * macros to get a ref to each of the 3 parts of the shared memory 
 * (bitmap, ref counts, data) because we cannot store pointer to 
 * them in the header since this memory is shared across virt
 * address spaces...
 */
#define SHM_BM_BITM(shm) ((word_t *)  (shm + ((struct shm_bm_header *) shm)->bitm_offset))
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

    for (i = 0; i < ind; i++)
        bm[i] = ~0x0ul;

    n = SHM_BM_BITMAP_BLOCK - offset;
    bm[ind] = ~((1ul << n) - 1); // set most sig n bits of bm[ind]
}

static inline int
shm_bm_clz(word_t *bm, void *stop, int *index, int *offset)
{
    int cnt, lz = 0;
    int ind = 0;
    
    while ((void *) bm < stop) {
        cnt = __builtin_clzl(*bm);
        lz += cnt;
        if (cnt != SHM_BM_BITMAP_BLOCK) {
            *index = ind;
            *offset = SHM_BM_BITMAP_BLOCK - cnt - 1;
            return lz;
        }
        bm++;
        ind++;
    }

    return -1;
}

/**
 * Creates a shared shared memory region of size `allocsz` from
 * which objects of size `objsz` can be allocated from. 
 *
 * Arguments:
 * - @shm     a pointer to a shm_bm_t that is set by function
 * - @objsz   size of objects that are allocated from this buffer
 * - @allocsz size of the shared memory buffer
 *
 * @return: a cbuf_t to identify the shared memory. 0 on failure
 */
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

    alloc = PAGE_SIZE + (bitmap_sz + refcnt_sz + data_sz);
    if (alloc > SHM_BM_ALLOC_BOUNDRY) return 0;

    id  = memmgr_shared_page_allocn(alloc/PAGE_SIZE, (vaddr_t *) *shm);
    if (id == 0) return 0;
    memset((void *) shm, 0, alloc);

    // metadata
    header = (struct shm_bm_header *) *shm;
    header->refcnt      = (refcnt_t) 1;
    header->objsz       = objsz;
    header->nobj        = nobj;
    header->bitm_offset = PAGE_SIZE; 
    header->refc_offset = PAGE_SIZE + bitmap_sz; 
    header->data_offset = PAGE_SIZE + bitmap_sz + refcnt_sz; 

    // make metadata read-only [NOT A FUNCTIONALITY PROVIDED BY CAPMGR?]
    // if (mprotect(*shm, PGSIZE-1, PROT_READ) == -1) {
    //     free(*shm);
    //     return -1;
    // }

    // set nobj bits to free
    sm_bm_set_contig(SHM_BM_BITM(*shm), nobj);
    return id;
}

/**
 * Maps a shared memory region identified by `id` into this component's
 * address space. The cbuf_t must have been created from a call to 
 * `shm_bm_create` in another component.
 *
 * Arguments:
 * - @id the cbuf_t that identifies a shared memory region
 *
 * @return: a shm_bm_t that references the shared memory region for 
 *          the calling component
 */
shm_bm_t 
shm_bm_map(cbuf_t id)
{
    shm_bm_t shm;
    memmgr_shared_page_map(id, (vaddr_t *) &shm);
    return shm;
}

/**
 * Allocates an object in the shared memory region reference by `shm`.
 *
 * Arguments:
 * - @shm the shared memory region from which to allocate an object
 * - @id  an indentifier that can be used to share this object between
 *        components if they have this shared memory region mapped
 *
 * @return: a pointer to the allocated object
 */
void *
shm_bm_obj_alloc(shm_bm_t shm, shm_bufid_t *id)
{
    int    freebit, idx, off;
    word_t word_old; 
    word_t *bm;

    // find a free space. could be preempted
    bm = SHM_BM_BITM(shm);
    do {
        freebit = shm_bm_clz(bm, SM_REFC(shm), &idx, &off);
        if (freebit == -1) return 0;
        word_old = bm[idx];
    } while (!cos_cas(bm + idx, word_old, word_old & ~(1ul << off)));

    cos_faa(SHM_BM_REFC(shm) + freebit, 1);

    *id = (shm_bufid_t) freebit;
    return SHM_BM_DATA(shm) + (freebit * SHM_BM_SIZE(shm));
}

/**
 * Takes a reference to an object identified by `id` allocated in 
 * a shared memory region. The shared memory region must be mapped
 * in the calling component's address space and the id must be to
 * an allocated object from a call to `shm_bm_obj_alloc` in another 
 * component. 
 *
 * Arguments:
 * - @shm the shared memory region from which to allocate an object
 * - @id  an indentifier that can be used to share this object between
 *        components if they have this shared memory region mapped
 *
 * @return: a pointer to the allocated object
 */
void *   
shm_bm_obj_use(shm_bm_t shm, shm_bufid_t id)
{
    if (id >= SHM_BM_NOBJ(shm)) return 0;

    // obj has not been allocated
    if (cos_faa(SHM_BM_REFC(shm) + id, 1) == 0) {
        cos_faa(SHM_BM_REFC(shm) + id, -1);
        return 0;
    }

    return SHM_BM_DATA(shm) + (id * SHM_BM_SIZE(shm));
}

/**
 * Drops a reference to an object allocated from a shared
 * memory region. If no other component is referencing that object,
 * it will be freed and can be reallocated. `ptr` should have come 
 * from a call to either `shm_bm_obj_alloc` or `shm_bm_obj_use`.
 * 
 * Arguments:
 * - @ptr a pointer to the object to free
 */
void
shm_bm_obj_free(void *ptr)
{
    (void) ptr;
}