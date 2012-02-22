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

	trans_ioctl_set_channel(fd, COS_TRANS_SERVICE_TERM);
	trans_ioctl_set_direction(fd, COS_TRANS_DIR_LTOC);
	a = mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (MAP_FAILED == a) {
		perror("mmap");
		exit(-1);
	}
	cringbuf_init(&sharedbuf, a, MAP_SIZE);
	
	while (1) {
		int off = 0;

		/* wait for user input */
		_read = read(0, buf, PRINT_CHUNK_SZ);
		assert(_read >= 0);
		do {
			int p;

			p = cringbuf_produce(&sharedbuf, buf + off, _read);
			_read -= p;
			off += p;
			if (p) write(fd, &c, 1);
		} while (_read);
	}

	if (munmap(a, MAP_SIZE) < 0) {
		perror("munmap");
		exit(-1);
	}

	return 0;
}
