#include <cos_types.h>
#include <string.h>
#include <arpa/inet.h>
#include <netmgr.h>
#include <netshmem.h>
#include <mc.h>
#include <cos_memcached.h>
#include <initargs.h>

unsigned long per_core_locktm[16] = {0};

void
mc_map_shmem(cbuf_t shm_id)
{
	netshmem_map_shmem(shm_id);
}

int
mc_conn_init(int proto)
{
	return cos_mc_new_conn(proto);
}

void print_payload(const unsigned char *data, int size) {
    for (int i = 0; i < size; i++) {
        // If character is printable, print it as is
        if (data[i] >= 32 && data[i] <= 126) {
            printf("%c", data[i]);
        }
        // Special handling for \r and \n
        else if (data[i] == '\r') {
            printf("\\r");
        }
        else if (data[i] == '\n') {
            printf("\\n");
        }
        // For non-printable characters, print them in hexadecimal
        else {
            printf("\\x%02X", data[i]);
        }
    }
    printf("\n");
}

typedef struct udphdr_s {
	uint16_t rqid;
	uint16_t partno;
	uint16_t nparts;
	uint16_t reserved;
} udphdr_t;

u16_t
mc_process_command(int fd, shm_bm_objid_t objid, u16_t data_offset, u16_t data_len)
{
	shm_bm_t shm = netshmem_get_shm();
	struct netshmem_pkt_buf *pkt_buf = shm_bm_borrow_net_pkt_buf(shm, objid);
	char *r_buf = (char *)pkt_buf + data_offset;
	//printc("%s\n", r_buf+sizeof(udphdr_t));
	char *w_buf = netshmem_get_data_buf(pkt_buf);
	//print_payload(r_buf, data_len);

	/* after this call, memcached should have data written into w_buf */
	return cos_mc_process_command(fd, r_buf, data_len, w_buf, netshmem_get_max_data_buf_sz());
}

unsigned long max = 0;
unsigned long cnt = 0;
void
mc_print(void)
{
	max = max > per_core_locktm[1] ? max : per_core_locktm[1];
	if (cos_cpuid() == 1) {
		cnt ++;
		if (cnt > 10000) {
			printc("locktm: %lu, max: %lu\n", per_core_locktm[1], max);
			per_core_locktm[1] = 0;
			cnt = 0;
		}
	}
	
}

void
cos_init(void)
{
	struct initargs params, curr;
	struct initargs_iter i;
	int argc, ret;
	char port_arg[20];
	int port = 11211;

	ret = args_get_entry("param", &params);
	assert(!ret);
	for (ret = args_iter(&params, &i, &curr); ret; ret = args_iter_next(&i, &curr)) {
		port = atoi(args_value(&curr));
		printc("len: %d, %d\n", args_len(&params), port);
	}

	sprintf(port_arg, "--udp-port=%d", port);
	//if (port == 11211) {
	char *argv[] =	{
		"--listen=10.10.2.2",
		"--port=0",// close tcp initialization
		port_arg,
		"--threads=512",
		"--conn-limit=4096",
		"--protocol=auto",
		"--memory-limit=64", //pre-allocation mode needs at least 39MB by default in Memcached
		"--extended=no_lru_crawler,no_lru_maintainer,no_hashexpand,no_slab_reassign,no_slab_automove",
	};

	printc("memcached port: %s\n", argv[2]);
	argc = ARRAY_SIZE(argv);

	/* 1. do initialization of memcached */
	ret = cos_mc_init(argc, argv);
	printc("memcached init done, ret: %d\n", ret);
}

int main()
{
	return 0;
}
