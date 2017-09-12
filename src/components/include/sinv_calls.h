#ifndef SINV_CALLS_H
#define SINV_CALLS_H

/* TODO this header needs to reorganized like sinv_calls and vk_hypercalls were */

/* Test Functions */
extern void * __inv_test_entry(int a, int b, int c, int d);
extern void * __inv_test_fs(int a, int b, int c, int d);
extern void * __inv_test_shdmem(int caller_spdid, int b, int c, int d);

/* Shared Memory Functions */
extern void * __inv_shdmem_get_vaddr(unsigned int spdid, int id, int arg3, int arg4);
extern void * __inv_shdmem_allocate(int spdid, int num_pages, int shmem_id, int arg4);
extern void * __inv_shdmem_deallocate(int a, int b, int c, int d);
extern void * __inv_shdmem_map(int a, int b, int c, int d);

#endif /* SINV_CALLS_H */
