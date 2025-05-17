
#include "image_operations.h"
#include "installs/native-stitcher/macos/x86_64/include/jpeg.h"

Image create_image(const char *filename) { return decompress_jpeg(filename); }

#define DEFINE_CREATE_IMAGE_FUNC(NAME, IMAGE_T, PIXEL_T)                       \
  IMAGE_T NAME(int width, int height, int channels) {                          \
    IMAGE_T img;                                                               \
    img.data = (PIXEL_T *)calloc(width * height * channels, sizeof(PIXEL_T));  \
    if (!img.data) {                                                           \
      return img;                                                              \
    }                                                                          \
    img.channels = channels;                                                   \
    img.width = width;                                                         \
    img.height = height;                                                       \
    return img;                                                                \
  }

DEFINE_CREATE_IMAGE_FUNC(create_empty_image, Image, unsigned char)
DEFINE_CREATE_IMAGE_FUNC(create_empty_image_s, ImageS, short)
DEFINE_CREATE_IMAGE_FUNC(create_empty_image_f, ImageF, float)

Image create_image_mask(int width, int height, float range, int left,
                        int right) {
  return create_mask(width, height, range, left, right);
}

int save_image(const Image *img, char *out_filename) {
  if (img->channels == CHANNELS) {
    return compress_jpeg(out_filename, img, 100);
  } else {
    return compress_grayscale_jpeg(out_filename, img, 100);
  }
}

void crop_image(Image *img, int cut_top, int cut_bottom, int cut_left,
                int cut_right) {
  crop_image_buf(img, cut_top, cut_bottom, cut_left, cut_right, img->channels);
}

#define DEFINE_DESTROY_IMAGE_FUNC(NAME, PIXEL_T)                               \
  void NAME(PIXEL_T *img) {                                                    \
    if (img->data != NULL) {                                                   \
      free(img->data);                                                         \
    }                                                                          \
  }

DEFINE_DESTROY_IMAGE_FUNC(destroy_image, Image)
DEFINE_DESTROY_IMAGE_FUNC(destroy_image_s, ImageS)
DEFINE_DESTROY_IMAGE_FUNC(destroy_image_f, ImageF)

#define DEFINE_IMAGE_SIZE_FUNC(NAME, IMAGE_T)                                  \
  int NAME(IMAGE_T *img) { return img->channels * img->height * img->width; }

DEFINE_IMAGE_SIZE_FUNC(image_size, Image)
DEFINE_IMAGE_SIZE_FUNC(image_size_s, ImageS)
DEFINE_IMAGE_SIZE_FUNC(image_size_f, ImageF)

Rect create_rect(int x, int y, int width, int height) {
  Rect rect;
  rect.x = x;
  rect.y = y;
  rect.width = width;
  rect.height = height;
  return rect;
}

Point br(Rect r) {
  Point result;
  result.x = r.x + r.width;
  result.y = r.y + r.height;
  return result;
}

#define DEFINE_DOWNSAMPLE_WORKER_FUNC(NAME, IMAGE_T, PIXEL_T)                  \
  void *NAME(void *args) {                                                     \
    ThreadArgs *arg = (ThreadArgs *)args;                                      \
    int start_row = arg->start_index;                                          \
    int end_row = arg->end_index;                                              \
    SamplingThreadData *data =                                                 \
        (SamplingThreadData *)arg->workerThreadArgs->std;                      \
    IMAGE_T *img = (IMAGE_T *)data->img;                                       \
    int imageSize = image_size(data->img);                                     \
    PIXEL_T *sampled = (PIXEL_T *)data->sampled;                               \
    for (int y = start_row; y < end_row; ++y) {                                \
      for (int x = 0; x < data->new_width; ++x) {                              \
        for (char c = 0; c < img->channels; ++c) {                             \
          float sum = 0.0;                                                     \
          for (int i = -2; i < 3; i++) {                                       \
            for (int j = -2; j < 3; j++) {                                     \
              int src_row = 2 * y + i;                                         \
              int src_col = 2 * x + j;                                         \
              int rr = reflect_index(src_row, img->height);                    \
              int cc = reflect_index(src_col, img->width);                     \
              int pos = (cc + rr * img->width) * img->channels + c;            \
              if (pos < imageSize) {                                           \
                sum += GAUSSIAN_KERNEL[i + 2][j + 2] * img->data[pos];         \
              }                                                                \
            }                                                                  \
          }                                                                    \
          if (data->image_type == IMAGE) {                                     \
            sum = (PIXEL_T)clamp(ceil(sum), 0, 255);                           \
          }                                                                    \
          sampled[(y * data->new_width + x) * img->channels + c] = sum;        \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    return NULL;                                                               \
  }

DEFINE_DOWNSAMPLE_WORKER_FUNC(down_sample_operation, Image, unsigned char)
DEFINE_DOWNSAMPLE_WORKER_FUNC(down_sample_operation_f, ImageF, float)
DEFINE_DOWNSAMPLE_WORKER_FUNC(down_sample_operation_s, ImageS, short)

#define DEFINE_DOWNSAMPLE_FUNC(NAME, IMAGE_T, PIXEL_T, IMAGE_T_ENUM)           \
  IMAGE_T NAME(IMAGE_T *img) {                                                 \
    IMAGE_T result;                                                            \
    int new_width = img->width / 2;                                            \
    int new_height = img->height / 2;                                          \
    PIXEL_T *downsampled = (PIXEL_T *)malloc(new_width * new_height *          \
                                             img->channels * sizeof(PIXEL_T)); \
    if (!downsampled) {                                                        \
      return result;                                                           \
    }                                                                          \
    SamplingThreadData std = {0,   new_width,   new_height,                    \
                              img, downsampled, IMAGE_T_ENUM};                 \
    WorkerThreadArgs wtd;                                                      \
    wtd.std = &std;                                                            \
    ParallelOperatorArgs args = {new_height, &wtd};                            \
    parallel_operator(DOWNSAMPLE, &args);                                      \
    result.channels = img->channels;                                           \
    result.data = downsampled;                                                 \
    result.width = new_width;                                                  \
    result.height = new_height;                                                \
    return result;                                                             \
  }

DEFINE_DOWNSAMPLE_FUNC(downsample, Image, unsigned char, IMAGE)
DEFINE_DOWNSAMPLE_FUNC(downsample_s, ImageS, short, IMAGES)
DEFINE_DOWNSAMPLE_FUNC(downsample_f, ImageF, float, IMAGEF)

#define DEFINE_UPSAMPLE_WORKER_FUNC(NAME, IMAGE_T, PIXEL_T)                    \
  void *NAME(void *args) {                                                     \
    ThreadArgs *arg = (ThreadArgs *)args;                                      \
    int start_row = arg->start_index;                                          \
    int end_row = arg->end_index;                                              \
    SamplingThreadData *s = (SamplingThreadData *)arg->workerThreadArgs->std;  \
    IMAGE_T *img = (IMAGE_T *)s->img;                                          \
    PIXEL_T *sampled = (PIXEL_T *)s->sampled;                                  \
    int pad = 2;                                                               \
    for (int y = start_row; y < end_row; ++y) {                                \
      for (int x = 0; x < s->new_width; ++x) {                                 \
        for (char c = 0; c < img->channels; ++c) {                             \
          float sum = 0;                                                       \
          for (int ki = 0; ki < 5; ki++) {                                     \
            for (int kj = 0; kj < 5; kj++) {                                   \
              int src_i = reflect_index(y + ki - pad, s->new_height);          \
              int src_j = reflect_index(x + kj - pad, s->new_width);           \
              int pixel_val = 0;                                               \
              if (src_i % 2 == 0 && src_j % 2 == 0) {                          \
                int orig_i = src_i / 2;                                        \
                int orig_j = src_j / 2;                                        \
                int image_pos =                                                \
                    (orig_i * img->width + orig_j) * img->channels + c;        \
                pixel_val = img->data[image_pos] * s->upsample_factor;         \
              }                                                                \
              sum += GAUSSIAN_KERNEL[ki][kj] * pixel_val;                      \
            }                                                                  \
          }                                                                    \
          int up_image_pos = (y * s->new_width + x) * img->channels + c;       \
          if (s->image_type == IMAGE) {                                        \
            sum = (PIXEL_T)clamp(floor(sum + 0.5), 0, 255);                    \
          }                                                                    \
          sampled[up_image_pos] = sum;                                         \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    return NULL;                                                               \
  }

DEFINE_UPSAMPLE_WORKER_FUNC(upsample_worker, Image, unsigned char)
DEFINE_UPSAMPLE_WORKER_FUNC(upsample_worker_s, ImageS, short)
DEFINE_UPSAMPLE_WORKER_FUNC(upsample_worker_f, ImageF, float)

#define DEFINE_UPSAMPLE_FUNC(NAME, IMAGE_T, PIXEL_T, IMAGE_T_ENUM)             \
  IMAGE_T NAME(IMAGE_T *img, float upsample_factor) {                          \
    IMAGE_T result;                                                            \
    int new_width = img->width * 2;                                            \
    int new_height = img->height * 2;                                          \
    PIXEL_T *upsampled = (PIXEL_T *)calloc(                                    \
        new_width * new_height * img->channels, sizeof(PIXEL_T));              \
    if (!upsampled) {                                                          \
      result.data = NULL;                                                      \
      result.width = result.height = result.channels = 0;                      \
      return result;                                                           \
    }                                                                          \
    SamplingThreadData std = {upsample_factor, new_width,   new_height, img,   \
                              upsampled,       IMAGE_T_ENUM};                  \
    WorkerThreadArgs wtd;                                                      \
    wtd.std = &std;                                                            \
    ParallelOperatorArgs args = {new_height, &wtd};                            \
    parallel_operator(UPSAMPLE, &args);                                        \
    result.data = upsampled;                                                   \
    result.width = new_width;                                                  \
    result.height = new_height;                                                \
    result.channels = img->channels;                                           \
    return result;                                                             \
  }

DEFINE_UPSAMPLE_FUNC(upsample, Image, unsigned char, IMAGE)
DEFINE_UPSAMPLE_FUNC(upsample_image_s, ImageS, short, IMAGES)
DEFINE_UPSAMPLE_FUNC(upsample_image_f, ImageF, float, IMAGES)
