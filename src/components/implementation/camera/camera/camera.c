#include <llprint.h>

#include <camera.h>
#include <shdmem.h>
#include <posix.h>
#include <sl.h>
#include <sl_lock.h>
#include <sl_thd.h>

#include <locale.h>
#include <limits.h>
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define NORTH 0
#define EAST 1
#define SOUTH 2
#define WEST 3

#include "jpeglib.h"

extern const void* _binary_image_jpg_start;
extern const void* _binary_image_jpg_end;
extern const int _binary_image_jpg_size;

struct rp {
	int x, y;
	unsigned long direction;
};
struct rp rpos;

int
check_location_image(int x, int y) {
	printc("check it\n");
	return 0;
}

void
cos_init(void)
{
	printc("Welcome to the robot_cont component\n");


	int shdmem_id;
	vaddr_t shdmem_addr;
	void *addr;
	u32_t addrlen;

	char * test = (char *)malloc(sizeof(char) * 11);
	test = "malloc test";
	printc("test: %s \n", test);
	
	printc("image size: %d\n", &_binary_image_jpg_size);
	printc("image start: %p\n", _binary_image_jpg_start);
	printc("image end: %p\n", _binary_image_jpg_end);

	struct jpeg_decompress_struct cinfo;
	jpeg_create_decompress(&cinfo);

	rpos.x = 0;
	rpos.y = 0;
	rpos.direction = EAST;	
	
	cos_sinv(BOOT_CAPTBL_SINV_CAP, 1, 2, 3, 4);
}
