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

extern const char _binary_greenroomba_jpg_start;
extern const char _binary_greenroomba_jpg_end;
extern int _binary_greenroomba_jpg_size;

struct rp {
	int x, y;
	unsigned long direction;
};
struct rp rpos;

unsigned char *raw_image = NULL;
int width;
int height;
int bytes_per_pixel;   // or 1 for GRACYSCALE images
int color_space; //or JCS_GRAYSCALE for grayscale images 
int **map;
int *location;
int track[4] = {-1,-1,-1,-1};
int init_map(int height, int width) {
  int i;
  int **m = (int**)malloc(sizeof(int*)*width);
  for(i = 0; i < width; i++) {
    m[i] = (int*)malloc(sizeof(int)*height);
    memset(m[i], '5', height);
  }
  map = m;
  return 1;
}
int check_quadrant(int x, int y) {
  if(y >= 0 && y < height/2 && x >=0 && x < width/2) return 0;
  if(y >= 0 && y < height/2 && x > width/2 && x < width) return 1;
  if(y >=height/2 && y < height/2 && x >=width/2 && x < width) return 2;
  if(y >=height/2 && y < height && x >=0 && x < width/2) return 3;
  return -1;
}

int check_green(int *triple, int x, int y) {  //1 green, 0 not green
  if(triple[0] < 100 && triple[1] > 240 && triple[2] < 100) {
    map[x][y] = 1;
    return 1;
  } else {
    map[x][y] = 0;
    track[check_quadrant(x, y)] = 1;
    return 0;
  }
  return -1;
}



int
check_location_image(int x, int y) {
	printc("check it\n");
	return 0;
}

int read_jpeg_file(void) //char *filename )
{
 struct jpeg_decompress_struct cinfo;
 struct jpeg_error_mgr jerr;
 JSAMPROW row_pointer[1];
 JSAMPARRAY colormap;
 unsigned long location = 0;
 int i = 0;

 cinfo.quantize_colors = TRUE;
 cinfo.err = jpeg_std_error( &jerr );
 jpeg_create_decompress( &cinfo );
 jpeg_stdio_src( &cinfo, (FILE*) &_binary_greenroomba_jpg_start );
 jpeg_read_header( &cinfo, TRUE );

 jpeg_start_decompress( &cinfo );

 raw_image = (unsigned char*)malloc( cinfo.output_width*cinfo.output_height*cinfo.num_components );
 row_pointer[0] = (char *)malloc( cinfo.output_width*cinfo.num_components );
 height = cinfo.output_height;
 width = cinfo.output_width;
 init_map(height, width);
 int x = 0;
 int y = 0;
  
  printc("cinfo.image_height: %d\n", cinfo.image_height);
  while( cinfo.output_scanline < cinfo.image_height )
 {

  x = 0;
  int triple[3];
  jpeg_read_scanlines( &cinfo, row_pointer, 1 );
  printc("cinfo.num_components: %d\n", cinfo.num_components);
  for( i=0; (unsigned int)i<cinfo.image_width*cinfo.num_components;i++) {
   raw_image[location++] = row_pointer[0][i];
   
   triple[i%cinfo.num_components] = (int)row_pointer[0][i];
   int test = row_pointer[0][i];
   if(i%cinfo.num_components == 2) {
        check_green(triple, y, x);
        x++;
   }
  }
  y++;
 }

 jpeg_finish_decompress( &cinfo );
 jpeg_destroy_decompress( &cinfo );
 free( row_pointer[0] );
 printc("read_jpeg_file end\n");
 return 1;
}



void
cos_init(void)
{
	printc("Welcome to the robot_cont component\n");


//	int shdmem_id;
//	vaddr_t shdmem_addr;
//	void *addr;
//	u32_t addrlen;
//
//	char * test = (char *)malloc(sizeof(char) * 11);
//	test = "malloc test";
//	printc("test: %s \n", test);
//	
//	struct jpeg_decompress_struct cinfo;
//	jpeg_create_decompress(&cinfo);
//
//	printc("%d \n", __LINE__);
//	*test = 'h';
//	printc("Image  Size: %d\n", &_binary_greenroomba_jpg_size);
//	printc("Image Start Addr: %d\n",&_binary_greenroomba_jpg_start);
//	printc("Image End Addr: %d\n",&_binary_greenroomba_jpg_end);
////	jpeg_get_large (j_common_ptr cinfo, size_t sizeofobject);
//	read_jpeg_file();
//        int i;
//	 for(i=0; i <4; i++) {
//	  printc("%d ", track[i]);
//	  if(track[i] == 1) {
//	     printc("Quadrant %d,", i+1);
//	  }
//	 }
////	shdmem_id = shm_allocate(2, 1);	
////	printc("shdmem_id: %d\n", shdmem_id);
//	rpos.x = 0;
//	rpos.y = 0;
//	rpos.direction = EAST;	
	
	cos_sinv(BOOT_CAPTBL_SINV_CAP, INIT_DONE, 2, 3, 4);
}
