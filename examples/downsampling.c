#include <stdio.h>
#include <time.h>
#include <string.h>
#include "laplace_blending.h"
#include "utils.h"

// naive testing
int main()
{
    Image img_buf1 = create_image("../files/apple.jpeg");
    Image mask1 = create_image_mask(img_buf1.width, img_buf1.height, 0.1f, 0, 1);
    Image down = downsample(&img_buf1);
    if (save_image(&down, "downsampled.jpg"))
    {
        printf("downsampled image  saved \n");
    }

    Image up = upsample(&img_buf1);
    if (save_image(&up, "upsampled.jpg"))
    {
        printf("%d %d \n",up.height,up.width);
        printf("upsampled image  saved \n");
    }


    destroy_image(&img_buf1);
    destroy_image(&down);
    destroy_image(&up);
    destroy_image(&mask1);
}