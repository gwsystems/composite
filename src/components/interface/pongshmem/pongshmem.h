#ifndef PONGSHMEM_H
#define PONGSHMEM_H

#include <cos_component.h>
#include <shm_bm.h>
#include <shm_bm_static.h>

#define BENCH_ITER 2048

struct obj_test {
	int  id;
	char string[12];
};

void pongshmem_test_map(cbuf_t shmid);
void pongshmem_test_objread(shm_objid_t objid, int test_string);
void pongshmem_test_refcnt(shm_objid_t objid);

void pongshmem_bench_map_nonstatic(cbuf_t shmid);
void pongshmem_bench_map(cbuf_t shmid);
void pongshmem_bench_syncinv(unsigned long word);
void pongshmem_bench_objread_nonstatic(shm_objid_t objid);
void pongshmem_bench_objread(shm_objid_t objid);

#endif
