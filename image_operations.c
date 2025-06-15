
#include "image_operations.h"
#include "jpeg.h"
#include "simde/simde/x86/avx2.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

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

int save_image(const Image *img, const char *out_filename) {
  if (img->channels == RGB_CHANNELS) {
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

StitchRect create_rect(int x, int y, int width, int height) {
  StitchRect rect;
  rect.x = x;
  rect.y = y;
  rect.width = width;
  rect.height = height;
  return rect;
}

StitchPoint br(StitchRect r) {
  StitchPoint result;
  result.x = r.x + r.width;
  result.y = r.y + r.height;
  return result;
}

int convolve_1d_v_simd(int x, int width, int *row0, int *row1, int *row2,
                       int *row3, int *row4, unsigned char *out_row) {

  // v0 + v4 + 2v2 + 4(v1 + v3 + v2)
  for (; x < width - 8; x += 8) {
    simde__m256i r0 = simde_mm256_loadu_si256((const simde__m256i *)(row0 + x));
    simde__m256i r1 = simde_mm256_loadu_si256((const simde__m256i *)(row1 + x));
    simde__m256i r2 = simde_mm256_loadu_si256((const simde__m256i *)(row2 + x));
    simde__m256i r3 = simde_mm256_loadu_si256((const simde__m256i *)(row3 + x));
    simde__m256i r4 = simde_mm256_loadu_si256((const simde__m256i *)(row4 + x));

    simde__m256i sum = simde_mm256_add_epi32(r0, r4);
    sum = simde_mm256_add_epi32(sum, simde_mm256_slli_epi32(r2, 1));
    simde__m256i t = simde_mm256_add_epi32(simde_mm256_add_epi32(r1, r3), r2);
    sum = simde_mm256_add_epi32(sum, simde_mm256_slli_epi32(t, 2));

    const simde__m256i bias = simde_mm256_set1_epi32(128);
    sum = simde_mm256_add_epi32(sum, bias);
    sum = simde_mm256_srli_epi32(sum, 8);

    simde__m128i lo16 = simde_mm256_castsi256_si128(sum);
    simde__m128i hi16 = simde_mm256_extracti128_si256(sum, 1);
    simde__m128i packed = simde_mm_packs_epi32(lo16, hi16);
    simde__m128i out8 = simde_mm_packus_epi16(packed, packed);
    simde_mm_storel_epi64((simde__m128i *)out_row, out8);

    out_row += 8;
  }

  return x;
}

int convolve_1d_3c(int x, int width, unsigned char *cur_src, int src_width,
                   int *temp_out) {
  for (; x < width; ++x) {
    int xx = (x * 2);
    for (int c = 0; c < RGB_CHANNELS; c++) {
      int p0 = reflect_index(xx - 2, src_width),
          p1 = reflect_index(xx - 1, src_width), p2 = xx,
          p3 = reflect_index(xx + 1, src_width),
          p4 = reflect_index(xx + 2, src_width);
      int s0 = cur_src[p0 * RGB_CHANNELS + c];
      int s1 = cur_src[p1 * RGB_CHANNELS + c];
      int s2 = cur_src[p2 * RGB_CHANNELS + c];
      int s3 = cur_src[p3 * RGB_CHANNELS + c];
      int s4 = cur_src[p4 * RGB_CHANNELS + c];

      temp_out[0] = s0 + s1 * 4 + s2 * 6 + s3 * 4 + s4;
      ++temp_out;
    }
  }
  return x;
}

void char_convolve_3(int range_start, int range_end, int src_width,
                     int src_height, unsigned char *src, unsigned char *dst) {

  int y = range_start;
  int yy = y * 2;
  int width = src_width / 2, height = src_height / 2;

  unsigned char *rows[5] = {
      src + (reflect_index(yy - 2, src_height)) * src_width * RGB_CHANNELS,
      src + (reflect_index(yy - 1, src_height)) * src_width * RGB_CHANNELS,
      src + yy * src_width * RGB_CHANNELS,
      src + (reflect_index(yy + 1, src_height)) * src_width * RGB_CHANNELS,
      src + (reflect_index(yy + 2, src_height)) * src_width * RGB_CHANNELS};

  int *temp_dst_out = (int *)malloc(5 * width * RGB_CHANNELS * sizeof(int));
  if (!temp_dst_out)
    return;

  int cache[16];

  int *temp_dst_rows[5] = {temp_dst_out, temp_dst_out + (width * RGB_CHANNELS),
                           temp_dst_out + (2 * width * RGB_CHANNELS),
                           temp_dst_out + (3 * width * RGB_CHANNELS),
                           temp_dst_out + (4 * width * RGB_CHANNELS)};
  int s_y = -2;
  int e_y = 3;

  for (; y < range_end; y++) {

    for (; s_y < e_y; s_y++) {
      unsigned char *cur_src = rows[s_y + 2];
      int *temp_out = temp_dst_rows[s_y + 2];
      int x = 0;
      const unsigned char *src0 = cur_src;
      const unsigned char *src1 = cur_src + 3;
      const unsigned char *src2 = cur_src + 6;
      const unsigned char *src3 = cur_src + 9;
      const unsigned char *src4 = cur_src + 12;

      x = convolve_1d_3c(x, min(3, width), cur_src, src_width, temp_out);
      temp_out = temp_out + (x * 3);

      for (; x <= width - 3; x += 3) {
        simde__m256i a_16 = simde_mm256_cvtepu8_epi16(
            simde_mm_loadu_si128((const simde__m128i *)src0));

        simde__m256i b_16 = simde_mm256_slli_epi16(
            simde_mm256_cvtepu8_epi16(
                simde_mm_loadu_si128((const simde__m128i *)src1)),
            2);

        simde__m256i c_16 = simde_mm256_cvtepu8_epi16(
            simde_mm_loadu_si128((const simde__m128i *)src2));

        c_16 = simde_mm256_add_epi16(
            simde_mm256_add_epi16(simde_mm256_slli_epi16(c_16, 1), c_16),
            simde_mm256_add_epi16(simde_mm256_slli_epi16(c_16, 1), c_16));

        simde__m256i d_16 = simde_mm256_slli_epi16(
            simde_mm256_cvtepu8_epi16(
                simde_mm_loadu_si128((const simde__m128i *)src3)),
            2);

        simde__m256i e_16 = simde_mm256_cvtepu8_epi16(
            simde_mm_loadu_si128((const simde__m128i *)src4));

        simde__m256i sum = simde_mm256_add_epi16(
            a_16, simde_mm256_add_epi16(
                      b_16, simde_mm256_add_epi16(
                                c_16, simde_mm256_add_epi16(d_16, e_16))));

        simde__m256i lo_sum =
            simde_mm256_cvtepi16_epi32(simde_mm256_castsi256_si128(sum));
        simde__m256i hi_sum =
            simde_mm256_cvtepi16_epi32(simde_mm256_extracti128_si256(sum, 1));

        simde_mm256_storeu_si256((simde__m256i *)cache, lo_sum);
        simde_mm256_storeu_si256((simde__m256i *)(cache + 8), hi_sum);

        temp_out[0] = cache[0], temp_out[1] = cache[1], temp_out[2] = cache[2];
        temp_out[3] = cache[6], temp_out[4] = cache[7], temp_out[5] = cache[8];
        temp_out[6] = cache[12], temp_out[7] = cache[13],
        temp_out[8] = cache[14];

        temp_out += (3 * RGB_CHANNELS);
        src0 += (6 * RGB_CHANNELS), src1 += (6 * RGB_CHANNELS);
        src2 += (6 * RGB_CHANNELS);
        src3 += (6 * RGB_CHANNELS), src4 += (6 * RGB_CHANNELS);
      }

      convolve_1d_3c(x, width, cur_src, src_width, temp_out);
    }

    yy = y * 2;
    unsigned char *out_row = dst + (RGB_CHANNELS * width * y);

    int *row0 = temp_dst_rows[0], *row1 = temp_dst_rows[1],
        *row2 = temp_dst_rows[2], *row3 = temp_dst_rows[3],
        *row4 = temp_dst_rows[4];

    int xx =
        convolve_1d_v_simd(0, width * 3, row0, row1, row2, row3, row4, out_row);
    int x = xx / 3;
    out_row = out_row + (x * RGB_CHANNELS);

    for (; x < width; ++x) {
      int xx = x * RGB_CHANNELS;
      for (int c = 0; c < RGB_CHANNELS; c++) {
        out_row[0] = clamp((row0[xx + c] + row1[xx + c] * 4 + row2[xx + c] * 6 +
                            4 * row3[xx + c] + row4[xx + c]) >>
                               8,
                           0, 255);

        ++out_row;
      }
    }

    rows[0] = rows[2], rows[1] = rows[3], rows[2] = rows[4];
    rows[3] = src + (reflect_index(((y + 1) * 2) + 1, src_height)) *
                        (src_width * RGB_CHANNELS);
    rows[4] = src + (reflect_index(((y + 1) * 2) + 2, src_height)) *
                        (src_width * RGB_CHANNELS);

    int *temp1 = temp_dst_rows[0], *temp2 = temp_dst_rows[1];
    temp_dst_rows[0] = temp_dst_rows[2], temp_dst_rows[1] = temp_dst_rows[3],
    temp_dst_rows[2] = temp_dst_rows[4], temp_dst_rows[3] = temp1,
    temp_dst_rows[4] = temp2;

    s_y = 1;
  }

  free(temp_dst_out);
}

int convolve_1d_1c(int x, int width, unsigned char *cur_src, int src_width,
                   int *temp_out) {
  for (; x < width; ++x) {
    int xx = x * 2;
    int s0 = cur_src[reflect_index(xx - 2, src_width)];
    int s1 = cur_src[reflect_index(xx - 1, src_width)];
    int s2 = cur_src[xx];
    int s3 = cur_src[reflect_index(xx + 1, src_width)];
    int s4 = cur_src[reflect_index(xx + 2, src_width)];

    temp_out[0] = s0 + s1 * 4 + s2 * 6 + s3 * 4 + s4;
    ++temp_out;
  }
  return x;
}

void char_convolve_1(int range_start, int range_end, int src_width,
                     int src_height, unsigned char *src, unsigned char *dst) {

  int y = range_start;
  int yy = y * 2;
  int width = src_width / 2, height = src_height / 2;

  unsigned char *rows[5] = {
      src + (reflect_index(yy - 2, src_height)) * src_width,
      src + (reflect_index(yy - 1, src_height)) * src_width,
      src + yy * src_width,
      src + (reflect_index(yy + 1, src_height)) * src_width,
      src + (reflect_index(yy + 2, src_height)) * src_width};

  int *temp_dst_out = (int *)malloc(5 * width * sizeof(int));
  if (!temp_dst_out)
    return;

  int *temp_dst_rows[5] = {
      temp_dst_out, temp_dst_out + width, temp_dst_out + (2 * width),
      temp_dst_out + (3 * width), temp_dst_out + (4 * width)};

  int s_y = -2;
  int e_y = 3;

  for (; y < range_end; y++) {
    for (; s_y < e_y; s_y++) {
      unsigned char *cur_src = rows[s_y + 2];
      int *temp_out = temp_dst_rows[s_y + 2];
      int x = 0;
      const unsigned char *src01 = cur_src;
      const unsigned char *src23 = cur_src + 2;
      const unsigned char *src4 = cur_src + 3;

      const simde__m256i w1_4 = simde_mm256_setr_epi16(1, 4, 1, 4, 1, 4, 1, 4,
                                                       1, 4, 1, 4, 1, 4, 1, 4);
      const simde__m256i w6_4 = simde_mm256_setr_epi16(6, 4, 6, 4, 6, 4, 6, 4,
                                                       6, 4, 6, 4, 6, 4, 6, 4);

      x = convolve_1d_1c(x, 1, cur_src, src_width, temp_out);
      temp_out = temp_out + x;
      for (; x < width - 8;
           x += 8, src01 += 16, src23 += 16, src4 += 16, temp_out += 8) {
        simde__m256i a01_16 = simde_mm256_cvtepu8_epi16(
            simde_mm_loadu_si128((const simde__m128i *)src01));
        simde__m256i m1 = simde_mm256_madd_epi16(a01_16, w1_4);

        simde__m256i a23_16 = simde_mm256_cvtepu8_epi16(
            simde_mm_loadu_si128((const simde__m128i *)src23));
        simde__m256i m2 = simde_mm256_madd_epi16(a23_16, w6_4);

        simde__m256i a4_16 = simde_mm256_cvtepu8_epi16(
            simde_mm_loadu_si128((const simde__m128i *)src4));
        simde__m256i fifth = simde_mm256_srli_epi32(a4_16, 16);

        simde__m256i sum =
            simde_mm256_add_epi32(simde_mm256_add_epi32(m1, m2), fifth);

        simde_mm256_storeu_si256((simde__m256i *)temp_out, sum);
      }

      convolve_1d_1c(x, width, cur_src, src_width, temp_out);
    }

    yy = y * 2;
    unsigned char *out_row = dst + (width * y);

    int *row0 = temp_dst_rows[0], *row1 = temp_dst_rows[1],
        *row2 = temp_dst_rows[2], *row3 = temp_dst_rows[3],
        *row4 = temp_dst_rows[4];

    int x = convolve_1d_v_simd(0, width, row0, row1, row2, row3, row4, out_row);
    out_row = out_row + x;

    for (; x < width; ++x) {
      out_row[0] = clamp(
          (row0[x] + row1[x] * 4 + row2[x] * 6 + 4 * row3[x] + row4[x]) >> 8, 0,
          255);

      ++out_row;
    }

    rows[0] = rows[2], rows[1] = rows[3], rows[2] = rows[4];
    rows[3] = src + (reflect_index(((y + 1) * 2) + 1, src_height)) * src_width;
    rows[4] = src + (reflect_index(((y + 1) * 2) + 2, src_height)) * src_width;

    int *temp1 = temp_dst_rows[0], *temp2 = temp_dst_rows[1];
    temp_dst_rows[0] = temp_dst_rows[2], temp_dst_rows[1] = temp_dst_rows[3],
    temp_dst_rows[2] = temp_dst_rows[4], temp_dst_rows[3] = temp1,
    temp_dst_rows[4] = temp2;

    s_y = 1;
  }

  free(temp_dst_out);
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
    if (data->image_type == IMAGE) {                                           \
      switch (img->channels) {                                                 \
      case GRAY_CHANNELS:                                                      \
        char_convolve_1(start_row, end_row, img->width, img->height,           \
                        (unsigned char *)img->data, (unsigned char *)sampled); \
        break;                                                                 \
      case RGB_CHANNELS:                                                       \
        char_convolve_3(start_row, end_row, img->width, img->height,           \
                        (unsigned char *)img->data, (unsigned char *)sampled); \
        break;                                                                 \
      default:                                                                 \
        break;                                                                 \
      }                                                                        \
    } else {                                                                   \
      for (int y = start_row; y < end_row; ++y) {                              \
        for (int x = 0; x < data->new_width; ++x) {                            \
          for (char c = 0; c < img->channels; ++c) {                           \
            float sum = 0.0;                                                   \
            for (int i = -2; i < 3; i++) {                                     \
              for (int j = -2; j < 3; j++) {                                   \
                int src_row = 2 * y + i;                                       \
                int src_col = 2 * x + j;                                       \
                int rr = reflect_index(src_row, img->height);                  \
                int cc = reflect_index(src_col, img->width);                   \
                int pos = (cc + rr * img->width) * img->channels + c;          \
                if (pos < imageSize) {                                         \
                  sum += GAUSSIAN_KERNEL[i + 2][j + 2] * img->data[pos];       \
                }                                                              \
              }                                                                \
            }                                                                  \
            if (data->image_type == IMAGE) {                                   \
              sum = (PIXEL_T)clamp(ceil(sum), 0, 255);                         \
            }                                                                  \
            sampled[(y * data->new_width + x) * img->channels + c] = sum;      \
          }                                                                    \
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

float get_pixel(float *image, int x, int y, int width, int height) {
  if (x < 0 || y < 0 || x >= width || y >= height)
    return FLT_MAX;
  return image[y * width + x];
}

void distance_transform(Image *mask) {
  assert(mask->channels == GRAY_CHANNELS);
  const float mask5[] = {1.0f, 1.4f};

  int x, y;
  ImageF dst = create_empty_image_f(mask->width, mask->height, mask->channels);

  for (int y = 0; y < mask->height; y++) {
    for (int x = 0; x < mask->width; x++) {
      dst.data[y * mask->width + x] =
          mask->data[y * mask->width + x] > 0 ? 0.0f : 255.0f;
    }
  }

float max = -1.0f;
  for (y = 0; y < mask->height; y++) {
    for (x = 0; x < mask->width; x++) {
      float current = dst.data[y * mask->width + x];
      if (current == 0.0f)
        continue;

      float min_val = current;

      min_val = fminf(min_val,
                      get_pixel(dst.data, x - 1, y, mask->width, mask->height) +
                          mask5[0]);
      min_val = fminf(min_val,
                      get_pixel(dst.data, x, y - 1, mask->width, mask->height) +
                          mask5[0]);
      min_val = fminf(min_val, get_pixel(dst.data, x - 1, y - 1, mask->width,
                                         mask->height) +
                                   mask5[1]);
      min_val = fminf(min_val, get_pixel(dst.data, x + 1, y - 1, mask->width,
                                         mask->height) +
                                   mask5[1]);
      min_val = fminf(min_val, get_pixel(dst.data, x - 2, y - 1, mask->width,
                                         mask->height) +
                                   mask5[0] + mask5[1]);
      min_val = fminf(min_val, get_pixel(dst.data, x - 1, y - 2, mask->width,
                                         mask->height) +
                                   mask5[0] + mask5[1]);

      dst.data[y * mask->width + x] = min_val;
    }
  }

  for (y = mask->height - 1; y >= 0; y--) {
    for (x = mask->width - 1; x >= 0; x--) {
      float current = dst.data[y * mask->width + x];

      float min_val = current;
      min_val = fminf(min_val,
                      get_pixel(dst.data, x + 1, y, mask->width, mask->height) +
                          mask5[0]);
      min_val = fminf(min_val,
                      get_pixel(dst.data, x, y + 1, mask->width, mask->height) +
                          mask5[0]);
      min_val = fminf(min_val, get_pixel(dst.data, x + 1, y + 1, mask->width,
                                         mask->height) +
                                   mask5[1]);
      min_val = fminf(min_val, get_pixel(dst.data, x - 1, y + 1, mask->width,
                                         mask->height) +
                                   mask5[1]);
      min_val = fminf(min_val, get_pixel(dst.data, x + 2, y + 1, mask->width,
                                         mask->height) +
                                   mask5[0] + mask5[1]);
      min_val = fminf(min_val, get_pixel(dst.data, x + 1, y + 2, mask->width,
                                         mask->height) +
                                   mask5[0] + mask5[1]);

      dst.data[y * mask->width + x] = min_val;
    }
  }


  for (int y = 0; y < mask->height; y++) {
    for (int x = 0; x < mask->width; x++) {

      mask->data[y * mask->width + x] = dst.data[y * mask->width + x] == 0.0f
                                            ? mask->data[y * mask->width + x]
                                            :  dst.data[y * mask->width + x];
    }
  }

  destroy_image_f(&dst);
}
