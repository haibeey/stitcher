

#include <stdio.h>
#include <time.h>
#include <string.h>
#include "blending.h"
#include "utils.h"

void test_multiband(){
    struct timespec start, end;
    double duration;

    Image img_buf1 = create_image("../files/apple.jpeg");
    Image img_buf2 = create_image("../files/orange.jpeg");

    Image mask1 = create_image_mask(img_buf1.width, img_buf1.height, 0.1f, 0, 1);
    Image mask2 = create_image_mask(img_buf2.width, img_buf2.height, 0.1f, 1, 0);

    int out = (img_buf1.width * 0.1f);
    int out_width = (img_buf1.width * 2) - (out * 2);
    StitchRect out_size = {0, 0, out_width, img_buf1.height};
    int num_bands = 5;

    clock_gettime(CLOCK_MONOTONIC, &start);
    Blender *b = create_blender(MULTIBAND,out_size, num_bands);
    clock_gettime(CLOCK_MONOTONIC, &end);
    duration = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Elapsed time for creating blended: %.2f seconds\n", duration);

    clock_gettime(CLOCK_MONOTONIC, &start);
    StitchPoint pt1 = {0, 0};
    feed(b, &img_buf1, &mask1, pt1);
    clock_gettime(CLOCK_MONOTONIC, &end);
    duration = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Elapsed time for feed 1: %.2f seconds\n", duration);

    clock_gettime(CLOCK_MONOTONIC, &start);
    StitchPoint pt2 = {img_buf2.width - out * 2  - 100 , 0};
    feed(b, &img_buf2, &mask2, pt2);
    clock_gettime(CLOCK_MONOTONIC, &end);
    duration = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Elapsed time for feed 2: %.2f seconds\n", duration);

    clock_gettime(CLOCK_MONOTONIC, &start);
    blend(b);
    clock_gettime(CLOCK_MONOTONIC, &end);
    duration = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Elapsed time for blend: %.2f seconds\n", duration);

    if (b->result.data != NULL)
    {
        if (save_image(&b->result, "merge.jpg"))
        {
            printf("Merged image  saved \n");
        }
    }

    destroy_blender(b);
    destroy_image(&img_buf1);
    destroy_image(&img_buf2);
    destroy_image(&mask1);
    destroy_image(&mask2);

}

void test_feather(){
    Image img_buf1 = create_image("../files/apple.jpeg");
    Image img_buf2 = create_image("../files/orange.jpeg");


    Image mask1 = create_image_mask(img_buf1.width, img_buf1.height, 0.1f, 0, 1);
    Image mask2 = create_image_mask(img_buf2.width, img_buf2.height, 0.1f, 1, 0);

    int out = (img_buf1.width * 0.1f);
    int out_width = (img_buf1.width * 2) - (out * 2);
    StitchRect out_size = {0, 0, out_width, img_buf1.height};

    Blender *b = create_blender(FEATHER,out_size, -1);

    StitchPoint pt1 = {0, 0};
    feed(b, &img_buf1, &mask1, pt1);

    StitchPoint pt2 = {img_buf2.width - out * 2  - 100 , 0};
    feed(b, &img_buf2, &mask2, pt2);

    blend(b);

    if (b->result.data != NULL)
    {
        if (save_image(&b->result, "merge_feather.jpg"))
        {
            printf("Merged image  saved \n");
        }
    }

    destroy_blender(b);
    destroy_image(&img_buf1);
    destroy_image(&img_buf2);
    destroy_image(&mask1);
    destroy_image(&mask2);


}
// naive testing
int main()
{
    test_multiband();
    test_feather();

    return 0;
}
