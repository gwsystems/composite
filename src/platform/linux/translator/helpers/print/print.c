#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define PROC_FILE "/proc/translator"
#define MAP_SIZE (4096 * 256)

int main(void)
{
	int fd;
	void *a;
//	const char *name = "print";

	fd = open(PROC_FILE, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(-1);
	}

	a = mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, fd, 0);
	if (MAP_FAILED == a) {
		perror("mmap");
		exit(-1);
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
