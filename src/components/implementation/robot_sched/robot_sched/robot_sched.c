#include <cos_component.h>
#include <cos_kernel_api.h>
#include <llprint.h>
#include <robot_cont.h>
#include <camera.h>
#include <cos_alloc.h>

int
assign_task(struct Task* task) {
	printc("In assign_task() in robot sched interface\n");

//	receive_task(task);
//	get_loc();
	return 1;
}


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


void
cos_init(void)
{
	int ret = 1;

	printc("Welcome to the robot sched component\n");
//	struct Task *t = (struct *Task)malloc(sizeof(struct Task));
//	t->command = 0;

//	read_jpeg_file();

//	diagonal(0,0);
//	assign_task(t);

	//read_jpeg_file(); //test readjpegfile
	cos_sinv(BOOT_CAPTBL_SINV_CAP, 1, 2, 3, 4);
	return;
}


