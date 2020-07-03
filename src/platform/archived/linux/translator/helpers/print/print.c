#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define LINUX_TEST
#include "../../translator_ioctl.h"
#include "../../../../../kernel/include/shared/cos_types.h"
#include "../../../../../components/include/cringbuf.h"
#include "../../../../../kernel/include/shared/cos_config.h"

#define PROC_FILE "/proc/translator"
#define MAP_SIZE  COS_PRINT_MEM_SZ //(4096 * 256)
#define PRINT_CHUNK_SZ (4096*4)

struct cringbuf sharedbuf;

int main(int argc, char **argv)
{
	int fd;
	void *a;
	char c, buf[PRINT_CHUNK_SZ];
	int channel;

	if (argc > 1) return -1;
	channel = COS_TRANS_SERVICE_PRINT;

	fd = open(PROC_FILE, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(-1);
	}

	trans_ioctl_set_channel(fd, channel);
	trans_ioctl_set_direction(fd, COS_TRANS_DIR_CTOL);
	a = mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (MAP_FAILED == a) {
		perror("mmap");
		exit(-1);
	}
	cringbuf_init(&sharedbuf, a, MAP_SIZE);
	
	while (1) {
		int amnt;
		/* wait for an event... */
		read(fd, &c, 1);
		do {
			amnt = cringbuf_consume(&sharedbuf, buf, PRINT_CHUNK_SZ);
			write(1 , buf, amnt); /* write to stdout */
		} while (amnt);
	}

	if (munmap(a, MAP_SIZE) < 0) {
		perror("munmap");
		exit(-1);
	}
	/* if (write(fd, name, strlen(name)) < 0) { */
	/* 	perror("read"); */
	/* 	exit(-1); */
	/* } */
	
	return 0;
}
