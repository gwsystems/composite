#ifndef PONGSHMEM_H
#define PONGSHMEM_H

#include <cos_component.h>
#include <shm_bm.h>

struct obj_test {
    char string[16];
};

void pongshmem_test_map(cbuf_t shmid);
void pongshmem_test_objread(shm_bufid_t objid, int test_string);

#endif
