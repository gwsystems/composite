#include <llprint.h>

#include <camera.h>
#include <cos_alloc.h>
#include <cos_component.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memmgr.h>

#include <sched.h>
#include <gateway_spec.h>

#include <udpserver.h>

#include "jpeglib.h"

#define JPEG_SZ 155803

#define NORTH 0
#define EAST 1
#define SOUTH 2
#define WEST 3

int image_available = 0;
struct cos_aep_info taeps;
char * buf;
int shdmem_id;
vaddr_t shdmem_addr;

char * jpeg_data_start;

struct rp {
	int x, y;
	unsigned long direction;
};
struct rp rpos;

int read_jpeg_file(void);

unsigned char *raw_image = NULL;
int width;
int height;
int bytes_per_pixel;   // or 1 for GRACYSCALE images
int color_space; //or JCS_GRAYSCALE for grayscale images 
int **map;
int *location;
int track[4] = {-1,-1,-1,-1};
int track9[9] = {0,0,0,0,0,0,0,0,0};
int obstacles[9] = {0,0,0,0,0,0,0,0,0};
int orange = 0;

extern const char _binary_greenroomba_jpg_start;
extern int _binary_greenroomba_jpg_size;

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

int 
check_quadrant(int x, int y) 
{
	if(y >= 0 && y < height/2 && x >=0 && x < width/2) return 0;
	if(y >= 0 && y < height/2 && x > width/2 && x < width) return 1;
	if(y >=height/2 && y < height/2 && x >=width/2 && x < width) return 2;
	if(y >=height/2 && y < height && x >=0 && x < width/2) return 3;
	return -1;
}

int 
check_section(int x, int y) 
{ //replacement for check_qudrant
	if(y >= 0 && y < height/3 && x >=0 && x < width/3) return 1;
	if(y >= 0 && y < height/3 && x > width/3 && x < 2*(width/3)) return 2;
	if(y >= 0 && y < height/3 && x >=2*(width/3) && x < width) return 3;
	if(y >= height/3 && y < 2*(height/3) && x >=0 && x < width/3) return 4;
	if(y >= height/3 && y < 2*(height/3) && x >=width/3 && x < 2*(width/3)) return 5;
	if(y >= height/3 && y < 2*(height/3) && x >=2*(width/3) && x < width) return 6;	
	if(y >= 2*(height/3) && y < height && x >=0 && x < width/3) return 7;	
	if(y >= 2*(height/3) && y < height && x >=width/3 && x < 2*(width/3)) return 8;
	if(y >= 2*(height/3) && y < height && x >=2*(width/3) && x < width) return 9;
	return -1;
}

/*
q1 q2 q3
q4 q5 q6
q7 q8 q9
00 10 20
01 11 21
02 12 22
*/
int 
check_obstacles(int *triple, int x, int y) 
{
	/*
	Checking for "obstacles"
	255-165-0 orange
	255-140-0 dark orange
	*/
	if(triple[0] > 240 && triple[1] >130 && triple[1] < 180 && triple[2] < 50) {
		obstacles[check_section(x,y)] = 1;
	}
	else {
	  	map[x][y] = 0;
	}
	return 0;
}


int 
ret_obstacles() 
{
	printc("ret_obstacles\n");
	
	//memset(obstacles, 0, 9*sizeof(int));
	orange = 1;
	read_jpeg_file();
	int i;
	orange = 0;
	for(i = 0; i < 9; i++) {
		if(obstacles[i]) return i;
	}
	return -1; //obstacles;
}

//new way of doing it, with 9 sections
int 
det_col(int *triple, int x, int y) 
{ //determine_color
	//check front color //green?
	if(triple[0] < 100 && triple[1] > 240 && triple[2] < 100) {
	   map[x][y] = 1;
	   track9[check_section(x,y)] = 1;
	   return 1;
	}
	
	//check back color //red?
	if(triple[0] > 240 && triple[1] < 100 && triple[2] < 100) {
	   map[x][y] = 2;
	   track9[check_section(x,y)] = 1;
	   return 2;
	}
	
	return 0;
}

int det_orient() { //determine_orientation
	int xc, yc;
	int i, j, k;
	printc("in det_orient\n");
	for(i = 0 ; i < width; i++) {
	  for(j = 0; j < height; j++) {
	      if (map[i][j] == 0) continue;
	      //for each col that one of the colors is in, check to see if the other color is in the same col
	      if (map[i][j] == 1) { // if green
	        for(k = j; k < height; k++) {
	/*NORTH*/  if (map[i][k] == 2) {
	      	 printc("NORTH\n");
	      	return 0; //green above red
	           }
	        }
	      } 
	      if (map[i][j] == 2) {
	        for(k = j; k < height; k++) {
	/*SOUTH*/  if (map[i][k] == 1) {
	      	printc("SOUTH\n"); 
	      	return 2; //green below red
	           }
	        }
	      } 
	      //repeat for the row
	      if (map[i][j] == 1) {
	        for(k = i; k < width; k++) {
	/*WEST*/ if (map[k][j] == 2) { 
	      	printc("WEST\n"); 
	      	return 3; //green left of red
	         }
	        }
	      } 
	      if (map[i][j] == 2) {
	        for(k = i; k < width; k++) {
	/*EAST*/ if (map[k][j] == 1) {
	      	printc("EAST\n"); 
	      	return 2; //green right of red
	         }
	        }
	      }
	  }
	}
	return -1; 
}

int
det_location_9(int x, int y) {

	read_jpeg_file();
	int i;
	for(i = 0 ; i< 9; i++) {
	  if(track9[i]) printc("Section %d\n", i);
	}
	printc("%d\n", det_orient());
	return 0;
}

int
printmap(void){
	int i;
	int j;
	for (i = 0; i < width; i++) {
		for(j = 0; j < height;j++) {
			printc("%d ", map[i][j]);
		}
		printc("\n");
	}
	return 0;
}

int 
read_jpeg_file(void) //char *filename )
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

	printc("read_jpeg\n");

	FILE * fp = fmemopen(jpeg_data_start, JPG_SZ, "rb");
	assert(fp);
	jpeg_stdio_src( &cinfo, fp);
	
	jpeg_read_header( &cinfo, TRUE );

	printc("start decompress with our real data\n");
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
		//printc("cinfo.num_components: %d\n", cinfo.num_components);
		for( i=0; (unsigned int)i<cinfo.image_width*cinfo.num_components;i++) {
			 raw_image[location++] = row_pointer[0][i];

		         if (row_pointer[0][i] < 0) {
		         	triple[i%cinfo.num_components] = 256 + (int)row_pointer[0][i];
			 } else {
		         	triple[i%cinfo.num_components] = (int)row_pointer[0][i];
			 }
		         int test = row_pointer[0][i];
		         if(i%cinfo.num_components == 2) {
		              //check_green(triple, y, x);
   		              if(!orange) det_col(triple, x, y);
			      else check_obstacles(triple, x, y); 
		              x++;
		         }
		}
	 	y++;
 	 }

 	jpeg_finish_decompress( &cinfo );
 	jpeg_destroy_decompress( &cinfo );
 
 return 1;
}

int
check_location_image(int x, int y) {

	if (!image_available) {
		return 0;
	}

	return 1;
}

void
camera_image_available(arcvcap_t rcv, void * data)
{
	printc("camera image available init\n");
	int ret;
	static int first = 1;

	while(1) {
		ret = cos_rcv(rcv, 0, NULL);
		assert(ret == 0);
		if (first) {
			first = 0;
			continue;
		}

		printc("camera image available\n");
		jpeg_data_start = (char *)shdmem_addr;
		printc("jpeg_data_start: %p \n", jpeg_data_start);
		//det_location_9(0, 0);
		ret_obstacles();
		image_available = 1;	
		
	}
}

void
cos_init(void)
{
	printc("\nWelcome to the Camera component\n");

	/* Create shared mem between Camera and udpserve */	
	shdmem_id = memmgr_shared_page_allocn(39, &shdmem_addr);
	assert(shdmem_id == CAMERA_UDP_SHMEM_ID && shdmem_addr);
	char *test = "testing camera shdmem";
	memcpy((char *)shdmem_addr, test, 21);

	/* Create AEP for requesting image from server */
	thdid_t tidp;
	int i = 0;
	tidp = sched_aep_create(&taeps, camera_image_available, (void *)i, 0, IMAGE_AEP_KEY);
	assert(tidp);
	sched_thd_param_set(tidp, sched_param_pack(SCHEDP_PRIO, AEP_PRIO));

	printc("Camera comp blocking\n");
	sched_thd_block(0);	
}
