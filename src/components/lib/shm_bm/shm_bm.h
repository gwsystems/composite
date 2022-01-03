#include <cos_types.h>
#include <memmgr.h>

typedef unsigned long shm_bm_t;    // opaque reference to the shared region
typedef unsigned int  shm_bufid_t; // an identifier to an allocated obj in the shared region

cbuf_t   shm_bm_create(shm_bm_t *shm, size_t objsz, size_t allocsz);
shm_bm_t shm_bm_map(cbuf_t id);

void *   shm_bm_obj_alloc(shm_bm_t shm, shm_bufid_t *id);
void *   shm_bm_obj_use(shm_bm_t shm, shm_bufid_t id);

void     shm_bm_obj_free(void *ptr);