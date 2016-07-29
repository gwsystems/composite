//header for shared mem ring buf

struct cos_shm_rb {
	unsigned int head, tail; 
	int vmid;
	unsigned int size;
	unsigned int mask;
	char buf[0];
};

struct cos_shm_rb * vk_shmem_addr_send(int vmid);

struct cos_shm_rb * vk_shmem_addr_recv(int vmid);

int vk_ringbuf_create(struct cos_compinfo *ci, struct cos_shm_rb * sm_rb, size_t tsize, int vmid);

int vk_ringbuf_enqueue(struct cos_shm_rb * rb, void * buff, size_t size);

int vk_ringbuf_dequeue(struct cos_shm_rb *rb, void * buff, size_t size);

int cos_shm_read(void *buff, size_t sz, unsigned int srcvm, unsigned int dstvm);

int cos_shm_write(void *buff, size_t sz, unsigned int srcvm, unsigned int dstvm);
