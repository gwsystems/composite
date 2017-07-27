#include <cos_component.h>
#include <stdio.h>
#include <print.h>
#include "../../../lib/libccv/ccv.h"
#include <print.h>

#define X_SLICE 1
#define Y_SLICE 1

void
cos_init(void)
{
        printc("Face Detection Test\n");

        ccv_enable_default_cache();
        ccv_dense_matrix_t* image = 0;
        ccv_bbf_classifier_cascade_t* cascade = ccv_bbf_read_classifier_cascade("face");
        ccv_read("photo.bmp", &image, CCV_IO_GRAY | CCV_IO_ANY_FILE);

        ccv_array_t* seq;

        seq = ccv_bbf_detect_objects(image, &cascade, 1, ccv_bbf_default_params);
        ccv_array_free(seq);

        int sliced_total = 0;
        int slice_rows = image->rows / Y_SLICE;
        int slice_cols = image->cols / X_SLICE;
        int i, count = X_SLICE * Y_SLICE;
        for (i = 0; i < count; i++) {
                int y = i / X_SLICE;
                int x = i - X_SLICE * y;
                ccv_dense_matrix_t* slice = 0;
                ccv_slice(image, (ccv_matrix_t**)&slice, 0, slice_rows * y, slice_cols * x, slice_rows, slice_cols);
                ccv_array_t* sseq = ccv_bbf_detect_objects(slice, &cascade, 1, ccv_bbf_default_params);
                sliced_total += sseq->rnum;
        }
        printc("detect %d faces\n", sliced_total);

        ccv_matrix_free(image);
        ccv_bbf_classifier_cascade_free(cascade);
        ccv_disable_cache();

        printc("done\n");

	return;
}
