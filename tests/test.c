
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "laplace_blending.h"
#include "utils.h"

int main()
{
    int data[16] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160};
    int expected[4] = {48, 59, 93, 104};
    int expected_up[16] = {62, 65, 69, 70,73,  76,  80,  82,90,  93,  97,  98,96,  99, 103, 104};
    Image mask1 = create_empty_image(4, 4, 1);
    for (int i = 0; i < 16; i++)
    {
        mask1.data[i] = data[i];
    }

    Image down = downsample(&mask1);
    if (image_size(&down) != 4)
    {
        printf("FATAL Image  size doesn't match expected expected 16 , got (%d)\n", image_size(&down));
        exit(1);
    }
    for (int i = 0; i < 4; i++)
    {
        if (down.data[i] != expected[i])
        {
            printf("FATAL Image  pixel  doesn't match expected (%d) got (%d)\n", expected[i], down.data[i]);
        }
    }

    Image up = upsample(&down);
    if (image_size(&up) != 16)
    {
        printf("FATAL Image  size doesn't match expected expected 16 , got (%d)\n", image_size(&down));
        exit(1);
    }
    for (int i = 0; i < 16; i++)
    {
        if (abs(up.data[i] - expected_up[i]) > 0)
        {
            printf("FATAL Image  pixel  doesn't match expected (%d) got (%d)\n", expected_up[i], up.data[i]);
            exit(1);
        }
    }

    int data2[48] = {249, 144, 1, 251, 146, 3, 254, 149, 6, 255, 152, 9, 250, 145, 2, 252, 147, 4, 254, 149,
                     6, 255, 151, 8, 252, 147, 4, 252, 147, 4, 253, 148, 5, 254, 149, 6, 253, 148, 5, 253, 148, 5, 253, 148, 5, 253, 148, 5};
    Image rgb_image = create_empty_image(4, 4, CHANNELS);
    for (int i = 0; i < image_size(&rgb_image); i++)
    {
        rgb_image.data[i] = data2[i];
    }

    Image down2 = downsample(&rgb_image);
    for (int i = 0; i < 4; i++)
    {
        int a = i * 3;
        printf("%d %d %d\n", down2.data[a], down2.data[a + 1], down2.data[a + 2]);
    }

    destroy_image(&mask1);
    destroy_image(&down);
    destroy_image(&rgb_image);
    return 0;
}