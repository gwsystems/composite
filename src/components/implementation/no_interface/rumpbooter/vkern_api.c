#include <cos_kernel_api.h>
#include <stdarg.h>
#include <stdio.h>
#include <vkern_api.h>

int
vk_recv_rb_create(struct cos_shm_rb * sm_rb, int vmid){
	assert(vmid < (COS_VIRT_MACH_COUNT-1));
	
	sm_rb = vk_shmem_addr_recv(vmid);
	sm_rb->head = 0;
	sm_rb->tail = 0;
	sm_rb->size = COS_SHM_VM_SZ/2 - (sizeof(struct cos_shm_rb *)*2);
	return 1;	
}

int
vk_send_rb_create(struct cos_shm_rb * sm_rb, int vmid){
	assert(vmid < (COS_VIRT_MACH_COUNT-1));
	
	sm_rb = vk_shmem_addr_send(vmid);
	sm_rb->head = 0;
	sm_rb->tail = 0;
	sm_rb->size = COS_SHM_VM_SZ/2 - (sizeof(struct cos_shm_rb *)*2);
	return 1;	
}

struct cos_shm_rb *
vk_shmem_addr_send(int to_vmid){
	if(to_vmid == 0){
		//if sending from a VM to DOM0
		return (struct cos_shm_rb *)BOOT_MEM_SHM_BASE;	
	}else{
		//if sending from DOM0 to a VM
		return (struct cos_shm_rb *)((BOOT_MEM_SHM_BASE)+((COS_SHM_VM_SZ) * (to_vmid-1) ));
	}
}

struct cos_shm_rb *
vk_shmem_addr_recv(int from_vmid){
	if(from_vmid == 0){
		//if rcving from DOM0
		return (struct cos_shm_rb *)((BOOT_MEM_SHM_BASE) + (COS_SHM_VM_SZ/2));	
	}else{
		//if DOM0 rcving from a VM
		return (struct cos_shm_rb *)((BOOT_MEM_SHM_BASE)+( (COS_SHM_VM_SZ) * (from_vmid-1) ) + ((COS_SHM_VM_SZ)/2));
	}
}

int
vk_ringbuf_isfull(struct cos_shm_rb *rb, size_t size){
	//doesn't account for wraparound, that's checked only if we need to wraparound.	
	if(rb->head+size >= rb->tail && rb->head < rb->tail){
		printc("rb full, rb->tail: %d, rb->head: %d\n", rb->tail, rb->head);
		return 1;
	}
	return 0;
}


int
vk_ringbuf_enqueue(struct cos_shm_rb *rb, void * buff, size_t size){
	if(vk_ringbuf_isfull(rb, size+1)){ return -1;} 
	unsigned int producer;
	producer = rb->head;

	//store size of entry
	rb->buf[producer] = size;
	producer ++;

	//check split wraparound
	if( producer+size > (rb->size) ){
		unsigned int first, second;
	
		first = rb->size - producer;
		second = size - first;
	
		//check if ringbuf is full w/ wraparound	
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
	} 
	unsigned int consumer = rb->tail;
	size_t size;

	//get size of entry	
	size = rb->buf[consumer];
	consumer++;
	
	//check for split wraparound
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

	assert(buff != NULL);
	return 1;
}

int
cos_shm_write(void *buff, size_t sz, unsigned int srcvm, unsigned int dstvm)
{
	assert(buff);
	struct cos_shm_rb * rb;

	//if you're DOM0, write to the rcv ringbuf of the VM you're sending to.
	//else if you're a VM, write to your send ringbuf for DOM0 to read from
	if (srcvm == 0) {
		rb = vk_shmem_addr_recv(dstvm);
	}else{
		rb = vk_shmem_addr_send(dstvm);
	}

	return vk_ringbuf_enqueue(rb,buff,sz);
}

int
cos_shm_read(void *buff, unsigned int srcvm, unsigned int curvm)
{
	assert(buff);
	struct cos_shm_rb * rb;

       /*
	* if you're a VM rcving from DOM0, read from your own rcv buf
	* else if you're DOM0 rcving from a VM, read from the VM's send buf.
	*/
	if (srcvm == 0) {
		rb = vk_shmem_addr_recv(0);
	}else{
		rb = vk_shmem_addr_send(srcvm);
	}
	
	return vk_ringbuf_dequeue(rb, buff);
}

