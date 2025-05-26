

#include "image_operations.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void test_sampling_operations() {
  int data[16] = {10, 20,  30,  40,
                  50,  60,  70,  80,
                  90, 100, 110, 120,
                  130, 140, 150, 160};
  int expected[4] = {48, 59, 93, 104};
  int expected_up[16] = {62, 65, 69, 70, 73, 76, 80,  82,
                         90, 93, 97, 98, 96, 99, 103, 104};
  Image mask1 = create_empty_image(4, 4, 1);
  for (int i = 0; i < 16; i++) {
    mask1.data[i] = data[i];
  }

  Image down = downsample(&mask1);
  if (image_size(&down) != 4) {
    printf("FATAL Image  size doesn't match expected expected 16 , got (%d)\n",
           image_size(&down));
    exit(1);
  }
  for (int i = 0; i < 4; i++) {
        if (abs(down.data[i] - expected[i]) > 2)
        {
          printf("FATAL Image  pixel  doesn't match expected (%d) got (%d)\n",
                 expected[i], down.data[i]);
        }
  }

  Image up = upsample(&down, 4.f);
  if (image_size(&up) != 16) {
    printf("FATAL Image  size doesn't match expected expected 16 , got (%d)\n",
           image_size(&down));
    exit(1);
  }
  for (int i = 0; i < 16; i++) {
    if (abs(up.data[i] - expected_up[i]) > 2) {
      printf("FATAL Image  pixel  doesn't match expected (%d) got (%d)\n",
             expected_up[i], up.data[i]);
      exit(1);
    }
  }

  int data2[48] = {249, 144, 1, 251, 146, 3, 254, 149, 6, 255, 152, 9,
                   250, 145, 2, 252, 147, 4, 254, 149, 6, 255, 151, 8,
                   252, 147, 4, 252, 147, 4, 253, 148, 5, 254, 149, 6,
                   253, 148, 5, 253, 148, 5, 253, 148, 5, 253, 148, 5};
  Image rgb_image = create_empty_image(4, 4, RGB_CHANNELS);
  for (int i = 0; i < image_size(&rgb_image); i++) {
    rgb_image.data[i] = data2[i];
  }

  unsigned char input_uc[4 * 4 * 3] = {
      10,  10,  10,  20,  20,  20,  30,  30,  30,  40,  40,  40,
      50,  50,  50,  60,  60,  60,  70,  70,  70,  80,  80,  80,
      90,  90,  90,  100, 100, 100, 110, 110, 110, 120, 120, 120,
      130, 130, 130, 140, 140, 140, 150, 150, 150, 160, 160, 160};

  for (int i = 0; i < image_size(&rgb_image); i++) {
    rgb_image.data[i] = input_uc[i];
  }

  Image out = downsample(&rgb_image);

  for (int i = 0; i < 2 * 2 * 3; i++) {
    printf("%d ", out.data[i]);
  }
  printf("\n");


  destroy_image(&out);
  destroy_image(&mask1);
  destroy_image(&down);
  destroy_image(&rgb_image);
}

int main() {

  Image img_buf1 = create_image("../files/apple.jpeg");
  Image mask = convert_RGB_to_gray(&img_buf1);

  Image dm = downsample(&mask);
  save_image(&dm, "testm1.jpg");

  ImageS imgs =
      create_empty_image_s(img_buf1.width, img_buf1.height, img_buf1.channels);

  convert_image_to_image_s(&img_buf1, &imgs);
  ImageS ds = downsample_s(&imgs);
  Image ds1 = create_empty_image(ds.width, ds.height, ds.channels);
  convert_images_to_image(&imgs, &img_buf1);
  convert_images_to_image(&ds, &ds1);

  save_image(&ds1, "test2.jpg");

  test_sampling_operations();

  destroy_image(&img_buf1);
  destroy_image_s(&imgs);
  destroy_image_s(&ds);
  destroy_image(&dm);

  Image bbbbb = create_image("/Users/abrahamakerele/p-h/lendostuff/lendo-image-processor/files/1.jpg");
  Image mask_bbbbb = convert_RGB_to_gray(&bbbbb);
  Image d = downsample(&bbbbb);
  printf("%d %d %d\n",d.width,d.height,d.channels);
  save_image(&d, "test1.jpg");

  destroy_image(&bbbbb);
  destroy_image(&mask_bbbbb);

  return 0;
}
