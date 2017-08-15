#include <cos_kernel_api.h>
#include <stdarg.h>
#include <stdio.h>
#include "cos2rk_rb_api.h"

struct mbuf; 
void rumpns_m_copydata(struct mbuf *m, int off, int len, void *vp);

int
cos2rk_recv_rb_create(struct cos2rk_shm_rb * sm_rb, int vmid){
	
	sm_rb = cos2rk_shmem_addr_recv(vmid);
	sm_rb->head = 0;
	sm_rb->tail = 0;
	sm_rb->size = COS2RK_SHM_VM_SZ/2 - (sizeof(struct cos2rk_shm_rb));
	return 1;	
}

int
cos2rk_send_rb_create(struct cos2rk_shm_rb * sm_rb, int vmid){
	
	sm_rb = cos2rk_shmem_addr_send(vmid);
	sm_rb->head = 0;
	sm_rb->tail = 0;
	sm_rb->size = COS2RK_SHM_VM_SZ/2 - (sizeof(struct cos2rk_shm_rb));
	return 1;	
}

int
cos2rk_rb_init(void)
{
	int i;

	for (i = 1; i < COS2RK_VIRT_MACH_COUNT; i ++) {
		struct cos2rk_shm_rb *snd_rb = NULL, *rcv_rb = NULL;

		cos2rk_send_rb_create(snd_rb, i);
		cos2rk_recv_rb_create(rcv_rb, i);
	}

	return 0;
}

struct cos2rk_shm_rb *
cos2rk_shmem_addr_send(int to_vmid){
	if(to_vmid == 0){
		/* if sending from a VM to DOM0 */
		return (struct cos2rk_shm_rb *)COS2RK_MEM_SHM_BASE;	
	}else{
		/* if sending from DOM0 to a VM */
		return (struct cos2rk_shm_rb *)((COS2RK_MEM_SHM_BASE)+((COS2RK_SHM_VM_SZ) * (to_vmid) ));
	}
}

struct cos2rk_shm_rb *
cos2rk_shmem_addr_recv(int from_vmid){
	if(from_vmid == 0){
		/* if rcving from DOM0 */
		return (struct cos2rk_shm_rb *)((COS2RK_MEM_SHM_BASE) + (COS2RK_SHM_VM_SZ/2));	
	}else{
		/* if DOM0 rcving from a VM */
		return (struct cos2rk_shm_rb *)((COS2RK_MEM_SHM_BASE)+( (COS2RK_SHM_VM_SZ) * (from_vmid) ) + ((COS2RK_SHM_VM_SZ)/2));
	}
}

int
cos2rk_ringbuf_isfull(struct cos2rk_shm_rb *rb, size_t size){

	/* doesn't account for wraparound, that's checked only if we need to wraparound. */
	if(rb->head+size >= rb->tail && rb->head < rb->tail){
		printc("rb full\n");
		return 1;
	}

	return 0;
}


int
cos2rk_ringbuf_enqueue(struct cos2rk_shm_rb *rb, void *buff, size_t size){
	
	unsigned int producer;

	assert(size > 0);

	if (cos2rk_ringbuf_isfull(rb, size+sizeof(size_t))) return -1;

	/* Assumption: if head == tail rb is empty */

	/* If there is not enough space at the end to store size goto start and skip last 4 bytes */
	if ((rb->size - rb->head) < sizeof(size_t))	{
		/* Check to see if ringbuffer is full if we add from start of rb */
		if (size+sizeof(size_t) >= rb->tail) return -1;
		/* skip the last few bytes at the end of rb */
		rb->head = 0;
	}

	producer = rb->head;

	memcpy(&rb->buf[producer], &size, sizeof(size_t));
	producer += sizeof(size_t);

	/* check split wraparound */
	if (unlikely(producer+size >= (rb->size))) {
		unsigned int first, second;
	
		first = rb->size - producer;
		second = size - first;

	
		/* check if ringbuf is full w/ wraparound */
		if (second >= rb->tail) {
			printc("wrap around, rb is full no enqueue\n");
			return -1;
		}

		rumpns_m_copydata((struct mbuf *)buff, 0, first, &rb->buf[producer]);
		producer = 0;
		rumpns_m_copydata((struct mbuf *)buff, first, second, &rb->buf[producer]);
	
		rb->head = producer+second;	
	} else {
		/* Normal case */
		rumpns_m_copydata((struct mbuf *)buff, 0, size, &rb->buf[producer]);
		__atomic_fetch_add(&rb->head, size+sizeof(size_t), __ATOMIC_SEQ_CST);
	}

	return 1;
}

int
cos2rk_dequeue_size(unsigned int srcvm, unsigned int curvm)
{
	struct cos2rk_shm_rb * rb;
	int ret;

	if (srcvm == 0) {
		rb = cos2rk_shmem_addr_recv(0);
	} else {
		rb = cos2rk_shmem_addr_send(srcvm);
	}
	

	if ((rb->size - rb->tail) < sizeof(size_t)) {
		/* Get the size from the head of the rb, skip last 4 bytes */
		memcpy(&ret, &rb->buf[0], sizeof(size_t));
	} else {
		/* Normal case */
		memcpy(&ret, &rb->buf[rb->tail], sizeof(size_t));
	}

	/* 
	 * If ret is a garbage value because RB is empty, return 0
	 * FIXME we should do something better than this, there is the chance that a
 	 * garbage value is in the valid range, then we waste time doing a malloc and imediately free after
	 * in cos_pktq_dequeue
	 */
	if (ret <= 0 || ret > 1500) ret = 0;
	return ret;
}

int
cos2rk_ringbuf_dequeue(struct cos2rk_shm_rb *rb, void * buff){

	unsigned int consumer;
	size_t size;

	/* Assumption: if head == tail rb is empty */

	if (rb->head == rb->tail) { 
		return -1; 
	} 

	/* If there is not enough space at the end to store size goto start and skip last 4 bytes */
	if ((rb->size - rb->tail) < sizeof(size_t))	{
		/* skip the last few bytes at the end of rb */
		rb->tail = 0;
		assert(rb->tail != rb->head);
	}

	consumer = rb->tail;

	/* get size of entry */
	memcpy(&size, &rb->buf[consumer], sizeof(size_t));
	consumer += sizeof(size_t);
	
	/* check for empty */
	assert(size > 0);

	/* check for split wraparound */
	if (unlikely(consumer+size >= rb->size)) {
		unsigned int first, second;

		first  = rb->size - consumer;
		second = size - first;


		/* tail is following head, it should only evert catch up at most */	
		assert(second <= rb->head);

		memcpy(buff, &rb->buf[consumer], first);
		consumer = 0;
		memcpy(buff+first, &rb->buf[consumer], second);
		
		rb->tail=consumer+second;
	} else {
		/* Normal case */
		memcpy(buff, &rb->buf[consumer], size);
		__atomic_fetch_add(&rb->tail, size+sizeof(size_t), __ATOMIC_SEQ_CST);
	}

	assert(buff != NULL);
	return 1;
}

int
cos2rk_shm_write(void *buff, size_t sz, unsigned int srcvm, unsigned int dstvm)
{
	assert(buff);
	struct cos2rk_shm_rb * rb;
	/* if you're DOM0, write to the rcv ringbuf of the VM you're sending to */
	/* else if you're a VM, write to your send ringbuf for DOM0 to read from */
	if (srcvm == 0) {
		rb = cos2rk_shmem_addr_recv(dstvm);
	}else{
		rb = cos2rk_shmem_addr_send(dstvm);
	}

	return cos2rk_ringbuf_enqueue(rb,buff,sz);
}


int
cos2rk_shm_read(void *buff, unsigned int srcvm, unsigned int curvm)
{
	assert(buff);
	struct cos2rk_shm_rb * rb;
       /*
	* if you're a VM rcving from DOM0, read from your own rcv buf
	* else if you're DOM0 rcving from a VM, read from the VM's send buf.
	*/
	if (srcvm == 0) {
		rb = cos2rk_shmem_addr_recv(0);
	}else{
		rb = cos2rk_shmem_addr_send(srcvm);
	}
	
	return cos2rk_ringbuf_dequeue(rb, buff);
}

