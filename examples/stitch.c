

#include <stdio.h>
#include "laplaceBlending.h"
#include "edJpeg.h"

int main()
{
    

    Image *img_buf1 = create_image("../files/bottom1.jpg");
    Image *img_buf2 = create_image("../files/bottom2.jpg");


    Image *mask1 = create_image_mask(img_buf1->width, img_buf1->height, 0.1f, 0, 1);
    Image *mask2 = create_image_mask(img_buf2->width, img_buf2->height, 0.1f, 1, 0);

    int cut = (int)(0.1f * img_buf1->width);
    Rect out_size = {0, 0, (img_buf1->width * 2) - (2 * cut), img_buf1->height};
    int num_bands = 5;

    Blender *b = create_blender(out_size, num_bands);
    Point pt1 = {0, 0};
    feed(b, img_buf1, mask1, pt1);
    Point pt2 = {img_buf2->width - (2 * cut), 0};
    feed(b, img_buf2, mask2, pt2);
    blend(b);

    if (save_image(b->result,"merge.jpg"))
    {
        printf("Merged image  saved \n");
    }

    destroy_blender(b);
    destroy_image(img_buf1);
    destroy_image(img_buf2);
    destroy_image(mask1);
    destroy_image(mask2);

    return 0;
}
