
int
cos_send_data(struct cos_compinfo *ci, void *buff, size_t sz, unsigned int srcvm, unsigned int dstvm)
{	
	assert(ci && buff);

	if(dstvm == 0){
		vk_ringbuf_enqueue((struct cos_shm_rb *)vk_shmem_addr_send(srcvm) ,buff, sz);
	}else{
		vk_ringbuf_enqueue((struct cos_shm_rb *)vk_shmem_addr_recv(dstvm) ,buff, sz);
	}
	
=======
cos_shm_read(struct cos_compinfo *ci, void *buff, size_t sz, unsigned int srcvm, unsigned int dstvm)
{
	vaddr_t data_vaddr = BOOT_MEM_SHM_BASE;

	assert(ci || buff);

	if (srcvm == 0) {
		data_vaddr += ((dstvm - 1) * COS_SHM_VM_SZ);
	}
	memcpy(buff, (void *)data_vaddr, sz);

>>>>>>> ea4d8702e005ab9515b20e1fe4d7a0aa784958fb
	return sz;
}

int
<<<<<<< HEAD
cos_recv_data(struct cos_compinfo *ci, void *buff, size_t sz, unsigned int srcvm, unsigned int dstvm)
{
	assert(ci || buff);

	if(srcvm == 0){
		vk_ringbuf_dequeue((struct cos_shm_rb *)vk_shmem_addr_recv(0), buff, sz);
	}else{	
		vk_ringbuf_dequeue((struct cos_shm_rb *)vk_shmem_addr_send(srcvm), buff, sz);
=======
cos_shm_write(struct cos_compinfo *ci, void *buff, size_t sz, unsigned int srcvm, unsigned int dstvm)
{
	vaddr_t data_vaddr = BOOT_MEM_SHM_BASE;

	assert(ci || buff);

	if (srcvm == 0) {
		data_vaddr += ((dstvm - 1) * COS_SHM_VM_SZ);
>>>>>>> ea4d8702e005ab9515b20e1fe4d7a0aa784958fb
	}
	
	memcpy((void *)data_vaddr, buff, sz);
