#include <stdio.h>
#include <time.h>
#include <string.h>
#include "laplace_blending.h"
#include "utils.h"
#include <stdlib.h>

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

    Image *img = &img_buf1;
    char buf[100];

    for (int i = 0; i < 3; i++)
    {
        Image down = downsample(img);
        sprintf(buf,"image%d.jpg",i);
        if (save_image(&down, buf))
        {
            printf("downsample image  saved \n");
        }
        free(img->data);
        img->data = down.data;
        img->width = down.width;
        img->height = down.height;
    }

    for (int i = 2; i >= 0; i--)
    {
        Image up = upsample(img);
        sprintf(buf,"image%d.jpg",i);
        if (save_image(&up, buf))
        {
            printf("upsample image  saved . shape %d %d %d \n",up.width,up.height,up.channels);
        }
        free(img->data);
        img->data = up.data;
        img->width = up.width;
        img->height = up.height;
    }



    destroy_image(&img_buf1);
    destroy_image(&down);
    destroy_image(&up);
    destroy_image(&mask1);
}
