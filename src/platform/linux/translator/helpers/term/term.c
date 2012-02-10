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
#define PRINT_CHUNK_SZ (4096*16)

struct cringbuf sharedbuf;

int main(void)
{
	int fd, _read = 0;
	void *a;
	char c, buf[PRINT_CHUNK_SZ];

	fd = open(PROC_FILE, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(-1);
	}

	trans_ioctl_set_channel(fd, 1);
	a = mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (MAP_FAILED == a) {
		perror("mmap");
		exit(-1);
	}
	cringbuf_init(&sharedbuf, a, MAP_SIZE);
	
	while (1) {
		int amnt, i;

		/* wait for user input */
		amnt = read(0, buf+_read, PRINT_CHUNK_SZ-_read);
		assert(amnt >= 0);
		_read += amnt;
		for (i = 0 ; i < _read ; i++) {
			if (buf[i] != '\n') continue;
			do {
				cringbuf_produce(&sharedbuf, buf, _read);
				write(fd, &c, 1);
			} while (amnt);
			_read = 0;
			break;
		}
	}

	if (munmap(a, MAP_SIZE) < 0) {
		perror("munmap");
		exit(-1);
	}

	return 0;
}
