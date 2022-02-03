#include <cos_types.h>
#include <memmgr.h>

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

/**
 * Opaque reference to the shared region to provide some abstraction. 
 * Functionally this is a pointer to the shared memory region's header.
 * This pointer is aligned on a power-of-2 boundary (see `shm_bm_obj_free`).
 */
typedef unsigned long shm_bm_t;

/**
 * An identifier to an allocated obj in the shared memory region. Allocating
 * an object from the shared memory region returns a `shm_bufid_t` that
 * identifies the object in the shared memory. Another component that is 
 * using the shared memory region can use this identifer to get a pointer to 
 * the object in the shared memory in their own address space.
 */
typedef unsigned int shm_bufid_t;

/**
 * Creates a shared shared memory region of size `allocsz` from
 * which objects of size `objsz` can be allocated. Internally,
 * objects are stored as a power of two so `objsz` is rounded up
 * to the nearest power of 2.
 *
 * Arguments:
 * - @shm     a pointer to a shm_bm_t that is set by function
 * - @objsz   size of objects that are allocated from this buffer
 * - @allocsz size of the shared memory buffer
 *
 * @return: a cbuf_t to identify the shared memory. 0 on failure
 */
cbuf_t shm_bm_create(shm_bm_t *shm, size_t objsz, int nobj);

/**
 * Maps a shared memory region identified by `id` into this component's
 * address space. The cbuf_t must have been created from a call to 
 * `shm_bm_create` in another component.
 *
 * Arguments:
 * - @id the cbuf_t that identifies a shared memory region
 *
 * @return: a shm_bm_t that references the shared memory region for 
 *          the calling component. 0 on failure.
 */
shm_bm_t shm_bm_map(cbuf_t id);

/**
 * Allocates an object in the shared memory region referenced by `shm`.
 *
 * Arguments:
 * - @shm the shared memory region from which to allocate an object
 * - @id  a pointer to an identifier that can be used to share this 
 *        object between components if they have this shared memory 
 *        region mapped. The value of the identifier is set by the 
 *        function
 *
 * @return: a pointer to the allocated object, 0 if no free objects
 */
void * shm_bm_obj_alloc(shm_bm_t shm, shm_bufid_t *id);

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
 * @return: a pointer to the allocated object. 0 on failure
 */
void * shm_bm_obj_use(shm_bm_t shm, shm_bufid_t id);

/**
 * Like `shm_bm_obj_use` except does not update the reference count
 * of the object. This can be used when the caller wants to 'borrow'
 * the object and not have to free it themselves, or when ownership 
 * of the object is being transfered to the caller so the reference 
 * count should stay the same.  
 *
 * Arguments:
 * - @shm the shared memory region from which to allocate an object
 * - @id  an indentifier that can be used to share this object between
 *        components if they have this shared memory region mapped
 *
 * @return: a pointer to the allocated object. 0 on failure
 */
void * shm_bm_obj_take(shm_bm_t shm, shm_bufid_t id);

/**
 * Drops a reference to an object allocated from a shared
 * memory region. If no other component is referencing that object,
 * it will be freed and can be reallocated. `ptr` should have come 
 * from a call to either `shm_bm_obj_alloc` or `shm_bm_obj_use`. 
 * Since shared memory region headers are aligned on a power of two,
 * getting a reference to the header from `ptr` us just a matter of 
 * masking out the bits of `ptr` that are less significant than the
 * allocation boundary.
 * 
 * Arguments:
 * - @ptr a pointer to the object to free
 */
void shm_bm_obj_free(void *ptr);
