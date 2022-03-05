#ifndef PONGSHMEM_H
#define PONGSHMEM_H

#include <cos_component.h>
#include <shm_bm.h>

#define BENCH_ITER 1024

struct obj_test {
	int  id;
	char string[12];
};

SHM_BM_INTERFACE_CREATE(testobj, sizeof (struct obj_test), BENCH_ITER);

void     pongshmem_test_map(cbuf_t shmid);
void     pongshmem_test_objread(shm_bm_objid_t objid, int test_string);
void     pongshmem_test_refcnt(shm_bm_objid_t objid);

void     pongshmem_bench_syncinv(unsigned long word);
void     pongshmem_bench_objread(shm_bm_objid_t objid);

#endif
