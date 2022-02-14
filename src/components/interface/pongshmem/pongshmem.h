#ifndef PONGSHMEM_H
#define PONGSHMEM_H

#include <cos_component.h>
#include <shm_bm.h>

#define BENCH_ITER 2048

struct obj_test {
	int  id;
	char string[12];
};

SHM_BM_INTERFACE_CREATE(testobj, sizeof (struct obj_test), BENCH_ITER);

shm_reqtok_t pongshmem_test_map(cbuf_t shmid);
void         pongshmem_test_objread(shm_reqtok_t reqtok, shm_objid_t objid, int test_string);
void         pongshmem_test_refcnt(shm_reqtok_t reqtok, shm_objid_t objid);

shm_reqtok_t pongshmem_bench_map(cbuf_t shmid);
void         pongshmem_bench_syncinv(unsigned long word);
void         pongshmem_bench_objread(shm_reqtok_t reqtok, shm_objid_t objid);

#endif
