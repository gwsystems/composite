#include <cos_component.h>
#include <cos_kernel_api.h>
#include <llprint.h>
#include <cos_types.h>
#include "../../lib/archives/libccv/ccv.h"
//#include "../../lib/archives/libccv/io/_ccv_io_libjpeg.c"
//#include <robot_sched.h>
#include <robot_cont.h>
#include <cos_alloc.h>

int
assign_task(struct Task* task) {
	printc("In assign_task() in robot sched interface\n");

//	receive_task(task);
//	get_loc();
	return 1;
}
/*
int read_jpeg_file(void) //char *filename )
{
 struct jpeg_decompress_struct cinfo;
 struct jpeg_error_mgr jerr;
 JSAMPROW row_pointer[1];
 JSAMPARRAY colormap;
 FILE *infile = fopen( "filename", "rb" );
 unsigned long location = 0;
 int i = 0;

 if ( !infile )
 {
  printf("Error opening jpeg file %s\n!", filename );
  return -1;
 }
 cinfo.quantize_colors = TRUE;
 cinfo.err = jpeg_std_error( &jerr );
 jpeg_create_decompress( &cinfo );
 jpeg_stdio_src( &cinfo, infile );
 jpeg_read_header( &cinfo, TRUE );
 jpeg_start_decompress( &cinfo );
 
 raw_image = (unsigned char*)malloc( cinfo.output_width*cinfo.output_height*cinfo.num_components );
 row_pointer[0] = (unsigned char *)malloc( cinfo.output_width*cinfo.num_components );
 height = cinfo.output_height; 
 width = cinfo.output_width;
 init_map(height, width);
 int x = 0;
 int y = 0;

  while( cinfo.output_scanline < cinfo.image_height )
 {

  x = 0;
  int triple[3];
  jpeg_read_scanlines( &cinfo, row_pointer, 1 );

  for( i=0; i<cinfo.image_width*cinfo.num_components;i++) {
   raw_image[location++] = row_pointer[0][i];
   triple[i%cinfo.num_components] = (int)row_pointer[0][i];  
   unsigned char test = row_pointer[0][i];
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
 fclose( infile );
 return 1;
}
*/
void
cos_init(void)
{
	int ret = 1;

	printc("Welcome to the robot sched component\n");
//	int *t = malloc(sizeof(4000));
//	t->command = 0;

	get_loc();

//	diagonal(0,0);
//	assign_task(t);

	//read_jpeg_file(); //test readjpegfile
	cos_sinv(BOOT_CAPTBL_SINV_CAP, 1, 2, 3, 4);
	return;
}


