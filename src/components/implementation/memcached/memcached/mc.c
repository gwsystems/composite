
#include <cos_types.h>
#include <string.h>
#include <arpa/inet.h>
#include <netmgr.h>
#include <netshmem.h>
#include <mc.h>

void
mc_map_shmem(cbuf_t shm_id)
{
	netshmem_map_shmem(shm_id);
}

int
mc_conn_init(void)
{
	return cos_mc_new_conn();
}

void
mc_process_command(int fd, shm_bm_objid_t objid, u16_t data_offset, u16_t data_len)
{
	cos_mc_process_command(fd, objid, data_offset, data_len);
}

void
cos_init(void)
{

}
