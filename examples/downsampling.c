#include "blending.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// naive testing
int main() {
  Image img_buf1 = create_image("../files/apple.jpeg");
  Image mask1 = create_image_mask(img_buf1.width, img_buf1.height, 0.1f, 0, 1);
  Image down = downsample(&img_buf1);
  if (save_image(&down, "downsampled.jpg")) {
    printf("downsampled image  saved \n");
  }

  Image up = upsample(&img_buf1, 4.f);
  if (save_image(&up, "upsampled.jpg")) {
    printf("%d %d \n", up.height, up.width);
    printf("upsampled image  saved \n");
  }

  Image *img = &img_buf1;
  char buf[100];

  for (int i = 0; i < 3; i++) {
    Image down = downsample(img);
    sprintf(buf, "image%d.jpg", i);
    if (save_image(&down, buf)) {
      printf("downsample image  saved \n");
    }
    free(img->data);
    img->data = down.data;
    img->width = down.width;
    img->height = down.height;
  }

  for (int i = 2; i >= 0; i--) {
    Image up = upsample(img, 4.f);
    sprintf(buf, "image%d.jpg", i);
    if (save_image(&up, buf)) {
      printf("upsample image  saved . shape %d %d %d \n", up.width, up.height,
             up.channels);
    }
    free(img->data);
    img->data = up.data;
    img->width = up.width;
    img->height = up.height;
  }

  Image img_ = create_image("./files/orange.jpeg");

  for (int i = 0; i < 10; i++) {
    Image down = downsample(&img_);
    printf("image downsample   %d %d %d \n", i + 1, down.width, down.height);
    free(img_.data);
    img_.data = down.data;
    img_.width = down.width;
    img_.height = down.height;
  }

  Image g = create_vertical_mask(3266, 3266, 0.5, 0, 1);

  destroy_image(&g);
  destroy_image(&img_buf1);
  destroy_image(&down);
  destroy_image(&up);
  destroy_image(&mask1);
  destroy_image(&img_);
}
