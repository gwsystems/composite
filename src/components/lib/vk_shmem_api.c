#include <cos_kernel_api.h>

/* HACKHACKHACKHACKHACKHACK */
#include <stdarg.h>
#include <stdio.h>
#include <vk_shmem_api.h>

int
vk_ringbuf_create(struct cos_compinfo *ci, struct cos_shm_rb * sm_rb, size_t tsize, int spdid){
	//create rb, of size sz
	sm_rb->head = 0;
	sm_rb->tail = 0;
	sm_rb->size = COS_SHM_VM_SZ/2;
	sm_rb->mask = sm_rb->size -1; 
	return 1;	
}

int 
vk_shmem_addr_send(int to_spdid){
	//returns the virt addr of the ringbuf struct for sending data
	if(to_spdid == 0){
		return BOOT_MEM_SHM_BASE;	
	}else{
		return ((BOOT_MEM_SHM_BASE)+((COS_SHM_VM_SZ) * (to_spdid-1) ));
	}
}

int 
vk_shmem_addr_recv(int from_spdid){
	//returns the virt addr of the ringbuf struct for recving data
	if(from_spdid == 0){
		return (BOOT_MEM_SHM_BASE) + (COS_SHM_VM_SZ/2);	
	}else{
		return (BOOT_MEM_SHM_BASE)+( (COS_SHM_VM_SZ) * (from_spdid-1) ) + ((COS_SHM_VM_SZ)/2) ;
	}
}

int
vk_ringbuf_isfull(struct cos_shm_rb *rb, size_t size){
	return ( ((rb->head+size) & rb->mask) == rb->tail & rb->mask );
}

int
vk_ringbuf_enqueue(struct cos_shm_rb *rb, void * buff, size_t size){
	if(vk_ringbuf_isfull(rb, size)){
		//TODO
		return -1;
	}
	
	memcpy(&rb->buf[rb->head], buff, size);
	__atomic_fetch_add(&rb->head, size, __ATOMIC_SEQ_CST);

	rb->head = (rb->head) & rb->mask;

	return 0;
}

int
cos_shm_read(void *buff, size_t sz, unsigned int srcvm, unsigned int dstvm)
{
	assert(buff);
	struct cos_shm_rb * rb;

	if (srcvm == 0) {
		rb = vk_shmem_addr_recv(0);
	}else{
		rb = vk_shmem_addr_send(srcvm);
	}
	
	return vk_ringbuf_dequeue(rb, buff, sz);

}

int
cos_shm_write(void *buff, size_t sz, unsigned int srcvm, unsigned int dstvm)
{

	assert(buff);
	struct cos_shm_rb * rb;

	if (srcvm == 0) {
		rb = vk_shmem_addr_send(srcvm);
	}else{
		rb = vk_shmem_addr_recv(dstvm);
	}

	return vk_ringbuf_enqueue((struct cos_shm_rb *)vk_shmem_addr_send(srcvm), buff, sz);
}
