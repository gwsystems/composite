#ifndef NICSHMEM_H
#define NICSHMEM_H

#include <cos_component.h>
#include <shm_bm.h>

#define PKT_BUF_NUM 4096
#define PKT_BUF_SIZE (PAGE_SIZE/2)

struct pkt_data_buf {
	volatile int flag;
	volatile int data_len;
	volatile char data[PKT_BUF_SIZE];
};

SHM_BM_INTERFACE_CREATE(shemem_data_buf, sizeof (struct pkt_data_buf), PKT_BUF_NUM);

void     nicshmem_test_map(cbuf_t shmid);
void     nicshmem_test_objread(shm_bm_objid_t objid, int test_string);

#endif
