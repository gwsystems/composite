#ifndef COS2RK_RB_API_H
#define COS2RK_RB_API_H
//header for shared mem ring buf
#include "cos2rk_types.h"

struct cos2rk_shm_rb {
	unsigned int head, tail; 
	int vmid;
	unsigned int size;
	char buf[0];
};

struct cos2rk_shm_rb * cos2rk_shmem_addr_send(int vmid);

struct cos2rk_shm_rb * cos2rk_shmem_addr_recv(int vmid);

int cos2rk_recv_rb_create(struct cos2rk_shm_rb * sm_rb, int vmid);
int cos2rk_send_rb_create(struct cos2rk_shm_rb * sm_rb, int vmid);

int cos2rk_dequeue_size(unsigned int srcvm, unsigned int curvm);

int cos2rk_ringbuf_enqueue(struct cos2rk_shm_rb * rb, void * buff, size_t size);

int cos2rk_ringbuf_dequeue(struct cos2rk_shm_rb *rb, void * buff);

int cos2rk_shm_read(void *buff, unsigned int srcvm, unsigned int dstvm);

int cos2rk_shm_write(void *buff, size_t sz, unsigned int srcvm, unsigned int dstvm);

#endif /* COS2RK_RB_API_H */
