#include <cos_kernel_api.h>

/* HACKHACKHACKHACKHACKHACK */
#include <stdarg.h>
#include <stdio.h>
#include <vkern_api.h>

int
vk_ringbuf_create(struct cos_compinfo *ci, struct cos_shm_rb * sm_rb, size_t tsize, int vmid){
	//create rb, of size sz
	sm_rb->head = 0;
	sm_rb->tail = 0;
	sm_rb->size = COS_SHM_VM_SZ/2 - (sizeof(struct cos_shm_rb *)*2);
	//sm_rb->mask = sm_rb->size -1; 
	return 1;	
}

struct cos_shm_rb *
vk_shmem_addr_send(int to_vmid){
	//returns the virt addr of the ringbuf struct for sending data
	if(to_vmid == 0){
		return (struct cos_shm_rb *)BOOT_MEM_SHM_BASE;	
	}else{
		return (struct cos_shm_rb *)((BOOT_MEM_SHM_BASE)+((COS_SHM_VM_SZ) * (to_vmid-1) ));
	}
}

struct cos_shm_rb *
vk_shmem_addr_recv(int vmid){
	//returns the virt addr of the ringbuf struct for recving data
	if(vmid == 0){
		return (struct cos_shm_rb *)((BOOT_MEM_SHM_BASE) + (COS_SHM_VM_SZ/2));	
	}else{
		return (struct cos_shm_rb *)((BOOT_MEM_SHM_BASE)+( (COS_SHM_VM_SZ) * (vmid-1) ) + ((COS_SHM_VM_SZ)/2));
	}
}

int
vk_ringbuf_isfull(struct cos_shm_rb *rb, size_t size){
/*	if((rb->head+size+1 & rb->mask) == (rb->tail & rb->mask)){
		printc("rb full, rb->tail: %d, rb->head: %d\n", rb->tail, rb->head);
		return 1;
	}*/
	if(rb->head+size+1 >= rb->tail && rb->head<rb->tail){
		printc("rb full, rb->tail: %d, rb->head: %d\n", rb->tail, rb->head);
		return 1;
	}
	return 0;
}


int
vk_ringbuf_enqueue(struct cos_shm_rb *rb, void * buff, size_t size){
	if(vk_ringbuf_isfull(rb, size+1)){ return -1;} //TODO

	unsigned int producer;
	producer = rb->head;

	//store size of entry
	rb->buf[producer] = size;
	producer ++;

	//get wraparound
	if( producer+size > (rb->size-(sizeof(rb)*2)) ){
		unsigned int first, second;
	
		first = rb->size - producer;
		second = size - first;
		
		if(rb->tail <= second) return -1; 

		memcpy(&rb->buf[producer], buff, first);
		
		producer = 0;
		memcpy(&rb->buf[producer], first+buff, second);
	
		rb->head = producer+second;	
	}else{
		memcpy(&rb->buf[producer], buff, size);
		__atomic_fetch_add(&rb->head, size+1, __ATOMIC_SEQ_CST);
	}

	return 1;
}

int
vk_ringbuf_dequeue(struct cos_shm_rb *rb, void * buff){
	if(rb->head == rb->tail){ 
		printc("rb is empty\n");
		return -1; 
	} //buffer is empty
	
	unsigned int consumer = rb->tail;
	size_t size;

	//get size of entry	
	size = rb->buf[consumer];
	consumer++;
	
	//check for wraparound
	if(consumer+size > rb->size){
		unsigned int first, second;
		
		first = rb->size - consumer;
		second = size - first;
	
		memcpy(buff, &rb->buf[consumer], first);
	
		consumer = 0;
		memcpy(buff+first, &rb->buf[consumer], second);
		
		rb->tail=consumer+second;
	}else{
		memcpy(buff, &rb->buf[consumer], size);
		__atomic_fetch_add(&rb->tail, size+1, __ATOMIC_SEQ_CST);
	}

	return 1;
}

int
cos_shm_write(void *buff, size_t sz, unsigned int srcvm, unsigned int dstvm)
{
	assert(buff);
	struct cos_shm_rb * rb;

	if (srcvm == 0) {
		rb = vk_shmem_addr_recv(dstvm);
	}else{
		rb = vk_shmem_addr_send(srcvm);
	}
	printc("cos_shm_write ret: %d\n:", vk_ringbuf_enqueue(rb,buff,sz));
	return 1;
}

int
cos_shm_read(void *buff, unsigned int srcvm, unsigned int curvm)
{
	struct cos_shm_rb * rb;

	if (srcvm == 0) {
		rb = vk_shmem_addr_recv(0);
	}else{
		rb = vk_shmem_addr_send(srcvm);
	}

	return vk_ringbuf_dequeue(rb, buff);
}

/*
int
vk_ringbuf_enqueue(struct cos_shm_rb *rb, void * buff, size_t size){
	printc("enqueue\n");
	if(vk_ringbuf_isfull(rb, size+1)){ return -1;} //TODO

	int tmp;

	rb->buf[rb->head] = size;
	memcpy(&rb->buf[(rb->head+1)], buff, size);
	//add if here for wraparound please	
	__atomic_fetch_add(&rb->head, size+1, __ATOMIC_SEQ_CST);
	
	tmp = (rb->head) & rb->mask;
	if(tmp < rb->head){
		rb->head = tmp + sizeof(rb);
	}else{
		rb->head = rb->head & rb->mask;
	}

	return 0;
}

int
vk_ringbuf_dequeue(struct cos_shm_rb *rb, void * buff){
	printc("dequeue\n");
	if(rb->head == rb->tail){ return -1; } //buffer is empty
	unsigned int tmp;
	size_t size;

	size = rb->buf[rb->tail];
	
	memcpy(buff, &rb->buf[rb->tail+1], size);
	
	__atomic_fetch_add(&rb->tail, size+1, __ATOMIC_SEQ_CST);
	
	tmp = rb->tail & rb->mask;
	if(tmp < rb->tail){
		rb->tail = tmp + sizeof(rb);
	}else{
		rb->tail = tmp;
	}
	return 1;
}
*/
