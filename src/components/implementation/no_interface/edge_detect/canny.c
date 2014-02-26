#include <cos_component.h>
#include <stdio.h>
#include <print.h>
#include "../../../lib/libccv/ccv.h"
#include <print.h>

#define CCV_CACHE_STAT
#define X_SLICE 1
#define Y_SLICE 1

void cos_ccv_merge(ccv_dense_matrix_t* mat[], ccv_dense_matrix_t** output, int rows, int cols, int x, int y);

void
cos_init(void *args)
{
	printc("Edge Detection Test\n");

	ccv_disable_cache();
	ccv_dense_matrix_t* yuv = 0;
	ccv_read("blackbox.bmp", &yuv, CCV_IO_GRAY | CCV_IO_ANY_FILE);

	ccv_dense_matrix_t* canny = 0;
	ccv_canny(yuv, &canny, 0, 3, 175, 320);
	ccv_matrix_free(canny);

        /*SLICE & DETECT*/
	int i, count = X_SLICE * Y_SLICE;
	int slice_rows = yuv->rows / Y_SLICE;
	int slice_cols = yuv->cols / X_SLICE;
	ccv_dense_matrix_t* canny_arr[count];
	for (i = 0; i < count; i++) {
		int y = i / X_SLICE;
		int x = i - X_SLICE * y;
		ccv_dense_matrix_t* slice = 0;
		ccv_slice(yuv, (ccv_matrix_t**)&slice, 0, slice_rows * y, slice_cols * x, slice_rows, slice_cols);
		canny_arr[i] = 0;
		ccv_canny(slice, &canny_arr[i], 0, 3, 175, 320);
	}

	/*MERGE*/
	ccv_dense_matrix_t* final_output = 0;
	cos_ccv_merge(canny_arr, &final_output, yuv->rows, yuv->cols, X_SLICE, Y_SLICE);
	ccv_matrix_free(final_output);
	ccv_matrix_free(yuv);


	printc("done\n");

	return;
}


void
cos_ccv_merge(ccv_dense_matrix_t* mat[], ccv_dense_matrix_t** output, int rows, int cols, int x, int y)
{
	ccv_enable_default_cache();
	ccv_dense_matrix_t* merged = 0;
	merged = ccv_dense_matrix_new(rows, cols, mat[0]->type, 0, 0);

	unsigned int i;
	unsigned int slice_rows = mat[0]->rows;
	unsigned int slice_step= mat[0]->step;
	unsigned int slice_num = x * y;
	unsigned int pixel_num = slice_rows * slice_step; 
	for (i = 0; i < slice_num; i++) {
		unsigned int x_offset = i % x;
		unsigned int y_offset = i / x;
		unsigned int j, mrow, mcol;
		for (j = 0; j < pixel_num; j++) {
			mrow = j / slice_step;
			mcol = j - slice_step * mrow;
			merged->data.u8[(y_offset * slice_rows + mrow) * merged->step + x_offset * slice_step + mcol] = mat[i]->data.u8[mrow * slice_step + mcol];
		}
	}
	*output = merged;
	ccv_disable_cache();

	return;
}
