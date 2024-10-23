#include <cos_types.h>
#include <memmgr.h>
#include <shm_bm.h>
#include <string.h>
#include <initargs.h>

int
pong_get_shmid(void)
{
    struct initargs shmem_entries,shmem_curr;
	struct initargs_iter i;
	int ret, cont;
	int shmem_cont, clients_cont;
	int shmid = 0xFF;
	ret = args_get_entry("virt_resources/shmem", &shmem_entries);
    assert(!ret);

	for (cont = args_iter(&shmem_entries, &i, &shmem_curr) ; cont ; cont = args_iter_next(&i, &shmem_curr)) { 		
		char *id_str = NULL;
		// get the ID
		id_str = args_get_from("id", &shmem_curr);
		shmid = atoi(id_str);
	}

    printc("Pong: Received shmid = %lu\n", shmid);

	return shmid;
}

void
pong_read_from_shared_memory(void)
{
    void *mem;
    unsigned long npages;
	int shmid = pong_get_shmid();

    // Map the shared memory provided by the manager
    npages = memmgr_shared_page_map_aligned(shmid, SHM_BM_ALIGN, (vaddr_t *)&mem);

    char received_message;
    
    // Read the byte from shared memory
    memcpy(&received_message, mem, sizeof(char));

    printc("Pong: Read '%c' from shared memory\n", received_message);

	if ( received_message == 'A') {
		printc("SUCCESS: ping and pong communicated through the shared memory created by capmgr");
	}
	else {
		printc("FAIL: no communication through the shared memory");
	}
}

int
main(void)
{
	pong_read_from_shared_memory();

	return 0;
}

