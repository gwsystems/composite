#include <cos_types.h>
#include <shm_bm.h>
#include <string.h>
#include <initargs.h>

int
ping_get_shmid(void)
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

    printc("Ping: Received shmid = %lu\n", shmid);
	return shmid;
}

void
ping_write_to_shared_memory(void)
{
    void *mem;
    unsigned long npages;
	int shmid = ping_get_shmid();

    // Map the shared memory provided by the manager
    npages = memmgr_shared_page_map_aligned(shmid, SHM_BM_ALIGN, (vaddr_t *)&mem);

    char *ping_message = "A"; // Example message to pass to pong
    printc("Ping: Writing '%c' to shared memory\n", *ping_message);

    // Write a byte to the shared memory
    memcpy(mem, ping_message, sizeof(char));
}

int
main(void)
{
	ping_write_to_shared_memory();

	return 0;
}
