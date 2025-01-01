

#include <stdio.h>
#include "laplaceBlending.h"
#include "edJpeg.h"
#include "utils.h"

int main()
{

    Image *img_buf1 = create_image("../files/orange.jpeg");
    Image *img_buf2 = create_image("../files/apple.jpeg");

    Image *mask1 = create_image_mask(img_buf1->width, img_buf1->height, 0.5f, 0, 1);
    Image *mask2 = create_image_mask(img_buf2->width, img_buf2->height, 0.6f, 1, 0);

    int out_width = img_buf1->width;
    Rect out_size = {0, 0, img_buf1->width, img_buf1->height};
    int num_bands = 3;

    Blender *b = create_blender(out_size, num_bands);
    Point pt1 = {0, 0};
    feed(b, img_buf1, mask1, pt1);
    Point pt2 = {0, 0};
    feed(b, img_buf2, mask2, pt2);
    blend(b);

    if (b->result != NULL)
    {
        crop_image(b->result, 0, 0, 0, 50);
        if (save_image(b->result, "merge.jpg"))
        {
            printf("Merged image  saved \n");
        }
    }

    destroy_blender(b);
    destroy_image(img_buf1);
    destroy_image(img_buf2);
    destroy_image(mask1);
    destroy_image(mask2);

    return 0;
}
