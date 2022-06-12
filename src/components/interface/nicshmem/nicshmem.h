#ifndef NICSHMEM_H
#define NICSHMEM_H

#include <cos_component.h>
#include <shm_bm.h>

#define PKT_BUF_NUM 4096
#define PKT_BUF_SIZE (1500)

struct data_buffer {
	char data[PKT_BUF_SIZE];
};

SHM_BM_INTERFACE_CREATE(data_buf, sizeof (struct data_buffer), PKT_BUF_NUM);

int nic_shemem_map(cbuf_t shmid);

#endif
