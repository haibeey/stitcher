#include "blending.h"
#include "jpeg.h"
#include "turbojpeg.h"
#include "utils.h"
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

Blender *create_multi_band_blender(Rect out_size, int nb) {

  Blender *blender = (Blender *)malloc(sizeof(Blender));
  blender->blender_type = MULTIBAND;
  if (!blender)
    return NULL;
  blender->real_out_size = out_size;

  blender->num_bands = min(MAX_BANDS, nb);

  double max_len = (double)(out_size.width > out_size.height ? out_size.width
                                                             : out_size.height);
  blender->num_bands =
      min(blender->num_bands, (int)ceil(log(max_len) / log(2.0)));

  out_size.width +=
      ((1 << blender->num_bands) - out_size.width % (1 << blender->num_bands)) %
      (1 << blender->num_bands);
  out_size.height += ((1 << blender->num_bands) -
                      out_size.height % (1 << blender->num_bands)) %
                     (1 << blender->num_bands);
  blender->output_size = out_size;

  blender->out = (ImageF *)malloc((blender->num_bands + 1) * sizeof(ImageF));
  blender->final_out =
      (ImageS *)malloc((blender->num_bands + 1) * sizeof(ImageS));
  blender->out_mask =
      (ImageF *)malloc((blender->num_bands + 1) * sizeof(ImageF));
  blender->out_width_levels =
      (int *)malloc((blender->num_bands + 1) * sizeof(int));
  blender->out_height_levels =
      (int *)malloc((blender->num_bands + 1) * sizeof(int));
  blender->img_laplacians =
      (ImageS *)malloc((blender->num_bands + 1) * sizeof(Image));
  blender->mask_gaussian =
      (ImageS *)malloc((blender->num_bands + 1) * sizeof(Image));

  if (!blender->out || !blender->final_out || !blender->out_mask ||
      !blender->out_width_levels || !blender->out_height_levels ||
      !blender->img_laplacians || !blender->mask_gaussian) {
    free(blender->out);
    free(blender->final_out);
    free(blender->out_mask);
    free(blender->out_width_levels);
    free(blender->out_height_levels);
    free(blender->img_laplacians);
    free(blender->mask_gaussian);
    free(blender);
    return NULL;
  }

  blender->out[0] = create_empty_image_f(out_size.width, out_size.height, 3);
  blender->out_mask[0] =
      create_empty_image_f(out_size.width, out_size.height, 1);

  blender->out_width_levels[0] = out_size.width;
  blender->out_height_levels[0] = out_size.height;

  for (int i = 1; i <= blender->num_bands; i++) {
    blender->out_width_levels[i] = (blender->out_width_levels[i - 1] + 1) / 2;
    blender->out_height_levels[i] = (blender->out_height_levels[i - 1] + 1) / 2;

    blender->out[i] = create_empty_image_f(blender->out_width_levels[i],
                                           blender->out_height_levels[i], 3);
    blender->out_mask[i] = create_empty_image_f(
        blender->out_width_levels[i], blender->out_height_levels[i], 1);
  }

  return blender;
}

Blender *create_feather_blender(Rect out_size) {
  Blender *blender = (Blender *)malloc(sizeof(Blender));
  blender->blender_type = FEATHER;
  if (!blender)
    return NULL;
  blender->real_out_size = out_size;
  blender->output_size = out_size;
  blender->sharpness = 2.5;
  blender->out_width_levels = NULL;
  blender->out_height_levels = NULL;
  blender->final_out = NULL;
  blender->img_laplacians = NULL;
  blender->mask_gaussian = NULL;

  blender->out = (ImageF *)malloc(sizeof(ImageF));
  blender->final_out = (ImageS *)malloc(sizeof(ImageS));
  blender->out_mask = (ImageF *)malloc(sizeof(ImageF));

  if (!blender->out || !blender->final_out || !blender->out_mask) {
    free(blender->out);
    free(blender->final_out);
    free(blender->out_mask);
    free(blender);
    return NULL;
  }

  blender->out[0] = create_empty_image_f(out_size.width, out_size.height, 3);
  blender->out_mask[0] =
      create_empty_image_f(out_size.width, out_size.height, 1);
  blender->do_distance_transform = 0;

  return blender;
}

Blender *create_blender(BlenderType blenderType, Rect out_size, int nb) {
  if (blenderType == MULTIBAND) {
    return create_multi_band_blender(out_size, nb);
  }
  return create_feather_blender(out_size);
}

void destroy_blender(Blender *blender) {
  if (!blender)
    return;

  if (blender->out != NULL) {
    free(blender->out);
  }

  if (blender->out_mask != NULL) {
    free(blender->out_mask);
  }

  if (blender->out_width_levels != NULL) {
    free(blender->out_width_levels);
  }

  if (blender->out_height_levels != NULL) {
    free(blender->out_height_levels);
  }
  destroy_image(&blender->result);
  if (blender->final_out != NULL) {
    destroy_image_s(blender->final_out);
  }

  if (blender->img_laplacians != NULL) {
    free(blender->img_laplacians);
  }
  if (blender->mask_gaussian != NULL) {
    free(blender->mask_gaussian);
  }
  free(blender);
}

void *compute_laplacian_worker(void *args) {
  ThreadArgs *arg = (ThreadArgs *)args;
  int start_row = arg->start_index;
  int end_row = arg->end_index;
  LaplacianThreadData *l = (LaplacianThreadData *)arg->workerThreadArgs->ltd;

  for (int i = start_row; i < end_row; ++i) {
    l->upsampled->data[i] = l->original->data[i] - l->upsampled->data[i];
  }

  return NULL;
}

void compute_laplacian(ImageS *original, ImageS *upsampled) {
  int total_size = original->width * original->height * original->channels;

  LaplacianThreadData ltd = {original, upsampled, total_size};
  WorkerThreadArgs wtd;
  wtd.ltd = &ltd;
  ParallelOperatorArgs args = {total_size, &wtd};

  parallel_operator(LAPLACIAN, &args);
}

void *feed_worker(void *args) {
  ThreadArgs *arg = (ThreadArgs *)args;
  int start_row = arg->start_index;
  int end_row = arg->end_index;
  FeedThreadData *f = (FeedThreadData *)arg->workerThreadArgs->ftd;

  for (int k = start_row; k < end_row; ++k) {
    for (int i = 0; i < f->cols; ++i) {
      int maskIndex = i + (k * f->level_width);
      int outMaskLevelIndex =
          ((i + f->x_tl) + ((k + f->y_tl) * f->out_level_width));

      for (char z = 0; z < RGB_CHANNELS; ++z) {
        int imgIndex = ((i + (k * f->level_width)) * RGB_CHANNELS) + z;

        if (imgIndex < f->img_laplacians[f->level].width *
                           f->img_laplacians[f->level].height * RGB_CHANNELS &&
            maskIndex < f->mask_gaussian[f->level].width *
                            f->mask_gaussian[f->level].height) {

          int outLevelIndex =
              ((i + f->x_tl) + (k + f->y_tl) * f->out_level_width) *
                  RGB_CHANNELS +
              z;

          float maskVal = f->mask_gaussian[f->level].data[maskIndex];
          float imgVal = f->img_laplacians[f->level].data[imgIndex];

          maskVal = maskVal * (1.0 / 255.0);

          imgVal = imgVal * maskVal;

          if (outLevelIndex <
                  f->out_level_height * f->out_level_width * RGB_CHANNELS &&
              outMaskLevelIndex < f->out_level_height * f->out_level_width) {
            f->out[f->level].data[outLevelIndex] += imgVal;

            if (z == 0) {
              f->out_mask[f->level].data[outMaskLevelIndex] += maskVal;
            }
          }
        }
      }
    }
  }

  return NULL;
}

int multi_band_feed(Blender *b, Image *img, Image *mask_img, Point tl) {
  ImageS images[b->num_bands + 1];
  int return_val = 1;

  int gap = 3 * (1 << b->num_bands);
  Point tl_new, br_new;

  tl_new.x = max(b->output_size.x, tl.x - gap);
  tl_new.y = max(b->output_size.y, tl.y - gap);

  Point br_point = br(b->output_size);
  br_new.x = min(br_point.x, tl.x + img->width + gap);
  br_new.y = min(br_point.y, tl.y + img->height + gap);

  tl_new.x = b->output_size.x +
             (((tl_new.x - b->output_size.x) >> b->num_bands) << b->num_bands);
  tl_new.y = b->output_size.y +
             (((tl_new.y - b->output_size.y) >> b->num_bands) << b->num_bands);

  int width = br_new.x - tl_new.x;
  int height = br_new.y - tl_new.y;

  width +=
      ((1 << b->num_bands) - width % (1 << b->num_bands)) % (1 << b->num_bands);
  height += ((1 << b->num_bands) - height % (1 << b->num_bands)) %
            (1 << b->num_bands);

  br_new.x = tl_new.x + width;
  br_new.y = tl_new.y + height;

  int dx = max(br_new.x - br_point.x, 0);
  int dy = max(br_new.y - br_point.y, 0);

  tl_new.x -= dx;
  br_new.x -= dx;
  tl_new.y -= dy;
  br_new.y -= dy;

  int top = tl.y - tl_new.y;
  int left = tl.x - tl_new.x;
  int bottom = br_new.y - tl.y - img->height;
  int right = br_new.x - tl.x - img->width;

  add_border_to_image(img, top, bottom, left, right, RGB_CHANNELS,
                      BORDER_REFLECT);
  add_border_to_image(mask_img, top, bottom, left, right, 1, BORDER_CONSTANT);

  images[0] = create_empty_image_s(img->width, img->height, img->channels);
  convert_image_to_image_s(img, &images[0]);
  for (int j = 0; j < b->num_bands; ++j) {
    images[j + 1] = downsample_s(&images[j]);
    if (!images[j + 1].data) {
      return_val = 0;
      goto clean;
    }

    b->img_laplacians[j] = upsample_image_s(&images[j + 1], 4.f);
    if (!&b->img_laplacians[j]) {
      return_val = 0;
      goto clean;
    }

    compute_laplacian(&images[j], &b->img_laplacians[j]);
  }

  b->img_laplacians[b->num_bands] = images[b->num_bands];
  ImageS sampled;
  ImageS mask_img_ = create_empty_image_s(mask_img->width, mask_img->height,
                                          mask_img->channels);
  convert_image_to_image_s(mask_img, &mask_img_);
  for (int j = 0; j < b->num_bands; ++j) {
    b->mask_gaussian[j] = mask_img_;
    sampled = downsample_s(&mask_img_);
    if (!sampled.data) {
      return_val = 0;
      goto clean;
    }
    mask_img_ = sampled;
  }

  b->mask_gaussian[b->num_bands] = mask_img_;

  int y_tl = tl_new.y - b->output_size.y;
  int y_br = br_new.y - b->output_size.y;
  int x_tl = tl_new.x - b->output_size.x;
  int x_br = br_new.x - b->output_size.x;

  for (int level = 0; level <= b->num_bands; ++level) {

    int rows = (y_br - y_tl);
    int cols = (x_br - x_tl);

    FeedThreadData ftd;
    ftd.rows = rows;
    ftd.cols = cols;
    ftd.x_tl = x_tl;
    ftd.y_tl = y_tl;
    ftd.out_level_width = b->out_width_levels[level];
    ftd.out_level_height = b->out_height_levels[level];
    ftd.level_width = b->img_laplacians[level].width;
    ftd.level_height = b->img_laplacians[level].height;
    ftd.level = level;
    ftd.img_laplacians = b->img_laplacians;
    ftd.mask_gaussian = b->mask_gaussian;
    ftd.out = b->out;
    ftd.out_mask = b->out_mask;

    WorkerThreadArgs wtd;
    wtd.ftd = &ftd;
    ParallelOperatorArgs args = {rows, &wtd};

    parallel_operator(FEED, &args);

    x_tl /= 2;
    y_tl /= 2;
    x_br /= 2;
    y_br /= 2;
  }
clean:
  for (size_t i = 0; i <= b->num_bands; i++) {
    destroy_image_s(&images[i]);
  }

  return return_val;
}

int feather_feed(Blender *b, Image *img, Image *mask_img, Point tl) {
  if (b->do_distance_transform) {
    distance_transform(mask_img);
  }

  for (int y = 0; y < img->height; y++) {
    for (int x = 0; x < img->width; x++) {
      int image_pos = ((y * img->width) + x) * RGB_CHANNELS;
      int mask_pos = (y * img->width) + x;
      int result_pos =
          ((x + tl.x) + ((y + tl.y) * b->output_size.width)) * RGB_CHANNELS;
      int result_mask_pos = ((x + tl.x) + ((y + tl.y) * b->output_size.width));
      float w = mask_img->data[mask_pos] / 256.0;
      b->out_mask->data[result_mask_pos] += w;
      for (int z = 0; z < RGB_CHANNELS; z++) {
        b->out->data[result_pos + z] += img->data[image_pos + z] * w;
      }
    }
  }

  return 1;
}

int feed(Blender *b, Image *img, Image *mask_img, Point tl) {
  assert(img->height == mask_img->height && img->width == mask_img->width);
  if (b->blender_type == MULTIBAND) {
    return multi_band_feed(b, img, mask_img, tl);
  } else {
    return feather_feed(b, img, mask_img, tl);
  }
}

void *blend_worker(void *args) {
  ThreadArgs *arg = (ThreadArgs *)args;
  int start_row = arg->start_index;
  int end_row = arg->end_index;
  BlendThreadData *b = (BlendThreadData *)arg->workerThreadArgs->btd;
  for (int i = start_row; i < end_row; ++i) {
    if (i < b->out_size) {
      b->blended_image.data[i] =
          b->blended_image.data[i] + b->out_level.data[i];
    }
  }
  return NULL;
}

void *normalize_worker(void *args) {
  ThreadArgs *arg = (ThreadArgs *)args;
  int start_row = arg->start_index;
  int end_row = arg->end_index;
  NormalThreadData *n = (NormalThreadData *)arg->workerThreadArgs->ntd;

  for (int y = start_row; y < end_row; ++y) {
    for (int x = 0; x < n->output_width; ++x) {
      int maskIndex = x + (y * n->output_width);
      if (maskIndex < image_size_f(&n->out_mask[n->level])) {
        float w = n->out_mask[n->level].data[maskIndex];

        for (char z = 0; z < RGB_CHANNELS; z++) {
          int imgIndex = (x + (y * n->output_width)) * RGB_CHANNELS + z;
          if (imgIndex < image_size_s(&n->final_out[n->level])) {

            n->final_out[n->level].data[imgIndex] =
                (short)(n->out[n->level].data[imgIndex] / (w + WEIGHT_EPS));
          }
        }
      }
    }
  }
  return NULL;
}

void multi_band_blend(Blender *b) {
  for (int level = 0; level <= b->num_bands; ++level) {
    b->final_out[level] = create_empty_image_s(
        b->out[level].width, b->out[level].height, b->out[level].channels);

    NormalThreadData ntd = {b->out[level].width, level, b->out, b->out_mask,
                            b->final_out};
    WorkerThreadArgs wtd;
    wtd.ntd = &ntd;
    ParallelOperatorArgs args = {b->out[level].height, &wtd};

    parallel_operator(NORMALIZE, &args);
    destroy_image_f(&b->out[level]);
    if (level > 0) {
      destroy_image_f(&b->out_mask[level]);
    }
  }

  ImageS blended_image = b->final_out[b->num_bands];

  for (int level = b->num_bands; level > 0; --level) {
    blended_image = upsample_image_s(&blended_image, 4.f);
    int out_size = image_size_s(&b->final_out[level - 1]);

    BlendThreadData btd = {out_size, blended_image, b->final_out[level - 1]};
    WorkerThreadArgs wtd;
    wtd.btd = &btd;
    ParallelOperatorArgs args = {out_size, &wtd};
    parallel_operator(BLEND, &args);
  }

  b->result.data =
      (unsigned char *)malloc(b->output_size.width * b->output_size.height *
                              RGB_CHANNELS * sizeof(unsigned char));
  b->result.channels = blended_image.channels;
  b->result.width = blended_image.width;
  b->result.height = blended_image.height;

  convert_images_to_image(&blended_image, &b->result);

  for (size_t i = 0; i < b->result.height; i++) {
    for (size_t j = 0; j < b->result.width; j++) {
      int pos = j + (i * b->result.width);
      float w = b->out_mask[0].data[pos];
      if (w <= WEIGHT_EPS) {
        int imgPos = (j + (i * b->result.width)) * RGB_CHANNELS;
        for (char c = 0; c < RGB_CHANNELS; c++) {
          b->result.data[imgPos + c] = 0;
        }
      }
    }
  }

  crop_image_buf(
      &b->result, 0, max(0, b->result.height - b->real_out_size.height), 0,
      max(0, b->result.width - b->real_out_size.width), RGB_CHANNELS);
  free(blended_image.data);

  destroy_image_s(&b->final_out[0]);
  b->final_out = NULL;
}

void feather_blend(Blender *b) {
  b->final_out[0] = create_empty_image_s(b->out[0].width, b->out[0].height,
                                         b->out[0].channels);

  NormalThreadData ntd = {b->out[0].width, 0, b->out, b->out_mask,
                          b->final_out};
  WorkerThreadArgs wtd;
  wtd.ntd = &ntd;
  ParallelOperatorArgs args = {b->out[0].height, &wtd};

  parallel_operator(NORMALIZE, &args);
  destroy_image_f(&b->out[0]);

  b->result.data =
      (unsigned char *)malloc(b->output_size.width * b->output_size.height *
                              RGB_CHANNELS * sizeof(unsigned char));

  b->result.channels = RGB_CHANNELS;
  b->result.width = b->output_size.width;
  b->result.height = b->output_size.height;

  convert_images_to_image(&b->final_out[0], &b->result);
  destroy_image_s(&b->final_out[0]);
  b->final_out = NULL;
}

void blend(Blender *b) {
  if (b->blender_type == MULTIBAND) {
    multi_band_blend(b);
  } else {
    feather_blend(b);
  }
}

void parallel_operator(OperatorType operatorType, ParallelOperatorArgs *arg) {
  int numThreads = get_cpus_count();
  int rowsPerThread = arg->rows / numThreads;
  int remainingRows = arg->rows % numThreads;

  pthread_t threads[numThreads];
  ThreadArgs thread_data[numThreads];

  int startRow = 0;
  for (unsigned int i = 0; i < numThreads; ++i) {
    int endRow = startRow + rowsPerThread + (remainingRows > 0 ? 1 : 0);
    if (remainingRows > 0) {
      --remainingRows;
    }

    switch (operatorType) {
    case UPSAMPLE:
    case DOWNSAMPLE:
      thread_data[i].end_index = endRow;
      thread_data[i].start_index = startRow;
      thread_data[i].workerThreadArgs = arg->workerThreadArgs;
      if (operatorType == DOWNSAMPLE) {
        switch (arg->workerThreadArgs->std->image_type) {
        case IMAGE:
          pthread_create(&threads[i], NULL, down_sample_operation,
                         &thread_data[i]);
          break;
        case IMAGES:
          pthread_create(&threads[i], NULL, down_sample_operation_s,
                         &thread_data[i]);
          break;
        case IMAGEF:
          pthread_create(&threads[i], NULL, down_sample_operation_f,
                         &thread_data[i]);
          break;
        default:
          break;
        }
      } else {
        switch (arg->workerThreadArgs->std->image_type) {
        case IMAGE:
          pthread_create(&threads[i], NULL, upsample_worker, &thread_data[i]);
          break;
        case IMAGES:
          pthread_create(&threads[i], NULL, upsample_worker_s, &thread_data[i]);
          break;
        case IMAGEF:
          pthread_create(&threads[i], NULL, upsample_worker_f, &thread_data[i]);
          break;
        default:
          break;
        }
      }

      break;
    case FEED:
      thread_data[i].end_index = endRow;
      thread_data[i].start_index = startRow;
      thread_data[i].workerThreadArgs = arg->workerThreadArgs;
      pthread_create(&threads[i], NULL, feed_worker, &thread_data[i]);
      break;
    case LAPLACIAN:
      thread_data[i].end_index = endRow;
      thread_data[i].start_index = startRow;
      thread_data[i].workerThreadArgs = arg->workerThreadArgs;
      pthread_create(&threads[i], NULL, compute_laplacian_worker,
                     &thread_data[i]);
      break;
    case BLEND:
      thread_data[i].end_index = endRow;
      thread_data[i].start_index = startRow;
      thread_data[i].workerThreadArgs = arg->workerThreadArgs;
      pthread_create(&threads[i], NULL, blend_worker, &thread_data[i]);
      break;
    case NORMALIZE:
      thread_data[i].end_index = endRow;
      thread_data[i].start_index = startRow;
      thread_data[i].workerThreadArgs = arg->workerThreadArgs;
      pthread_create(&threads[i], NULL, normalize_worker, &thread_data[i]);
      break;
    }

    startRow = endRow;
  }

  for (unsigned int i = 0; i < numThreads; ++i) {
    pthread_join(threads[i], NULL);
  }
}
