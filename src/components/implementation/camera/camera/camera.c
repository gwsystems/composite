//#include <cos_component.h>
//#include <cos_kernel_api.h>
//#include <video_codec.h>

//#include <cos_defkernel_api.h>
//#include <cos_alloc.h>
//#include <cos_debug.h>
//#include <cos_types.h>
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

	char * test = (char *)malloc(1);

	struct jpeg_decompress_struct cinfo;
	jpeg_create_decompress(&cinfo);

	printc("%d \n", __LINE__);
	*test = 'h';

	printc("%d \n", __LINE__);
	printc("test: %c \n", *test);
//	shdmem_id = shm_allocate(2, 1);	
//	printc("shdmem_id: %d\n", shdmem_id);

	rpos.x = 0;
	rpos.y = 0;
	rpos.direction = EAST;	
	
	cos_sinv(BOOT_CAPTBL_SINV_CAP, 1, 2, 3, 4);
}
