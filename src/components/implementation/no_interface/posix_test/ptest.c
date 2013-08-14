#include <cos_component.h>
#include <print.h>
#include "../../../lib/libccv/ccv.h"
#include <stdio.h>

void face_detect(void);

void
cos_init(void *args)
{
        face_detect();
        printc("tests finished\n");

        return;
}

void
face_detect(void)
{
        int i;
        char *argv[3];

        argv[0] = NULL;         /* take the position */
        argv[1] = "photo.bmp";  /* the picture file (.BMP) to be detected */
        argv[2] = "face";       /* the face data */

        ccv_enable_default_cache();
        ccv_dense_matrix_t* image = 0;
        ccv_bbf_classifier_cascade_t* cascade = ccv_load_bbf_classifier_cascade(argv[2]);
        ccv_read(argv[1], &image, CCV_IO_GRAY | CCV_IO_ANY_FILE);
        ccv_array_t* seq = ccv_bbf_detect_objects(image, &cascade, 1, ccv_bbf_default_params);
        for (i = 0; i < seq->rnum; i++) {
                ccv_comp_t* comp = (ccv_comp_t*)ccv_array_get(seq, i);
                printc("%d %d %d %d\n", comp->rect.x, comp->rect.y, comp->rect.width, comp->rect.height);
        }
        printc("total : %d\n", seq->rnum);

        printf("test\n");
        ccv_array_free(seq);
        ccv_matrix_free(image);
        ccv_bbf_classifier_cascade_free(cascade);
        ccv_disable_cache();

        return;
}
