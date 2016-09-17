//header for shared mem ring buf
#include "vk_types.h"

struct cos_shm_rb {
	unsigned int head, tail; 
	int vmid;
	unsigned int size;
	char buf[0];
};

struct cos_shm_rb * vk_shmem_addr_send(int vmid);

struct cos_shm_rb * vk_shmem_addr_recv(int vmid);

int vk_recv_rb_create(struct cos_shm_rb * sm_rb, int vmid);
int vk_send_rb_create(struct cos_shm_rb * sm_rb, int vmid);

int vk_ringbuf_enqueue(struct cos_shm_rb * rb, void * buff, size_t size);

int vk_ringbuf_dequeue(struct cos_shm_rb *rb, void * buff);

int cos_shm_read(void *buff, unsigned int srcvm, unsigned int dstvm);

int cos_shm_write(void *buff, size_t sz, unsigned int srcvm, unsigned int dstvm);
