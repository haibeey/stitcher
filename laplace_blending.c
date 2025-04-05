#include "turbojpeg.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "utils.h"
#include "laplace_blending.h"

static const float GAUSSIAN_KERNEL[5][5] = {
    {1.0 / 256, 4.0 / 256, 6.0 / 256, 4.0 / 256, 1.0 / 256},
    {4.0 / 256, 16.0 / 256, 24.0 / 256, 16.0 / 256, 4.0 / 256},
    {6.0 / 256, 24.0 / 256, 36.0 / 256, 24.0 / 256, 6.0 / 256},
    {4.0 / 256, 16.0 / 256, 24.0 / 256, 16.0 / 256, 4.0 / 256},
    {1.0 / 256, 4.0 / 256, 6.0 / 256, 4.0 / 256, 1.0 / 256}};

static const float WEIGHT_EPS = 1e-5f;

Image create_image(const char *filename)
{
    return decompress_jpeg(filename);
}

#define DEFINE_CREATE_IMAGE_FUNC(NAME, IMAGE_T, PIXEL_T)                          \
    IMAGE_T NAME(int width, int height, int channels)                             \
    {                                                                             \
        IMAGE_T img;                                                              \
        img.data = (PIXEL_T *)calloc(width * height * channels, sizeof(PIXEL_T)); \
        if (!img.data)                                                            \
        {                                                                         \
            return img;                                                           \
        }                                                                         \
        img.channels = channels;                                                  \
        img.width = width;                                                        \
        img.height = height;                                                      \
        return img;                                                               \
    }

DEFINE_CREATE_IMAGE_FUNC(create_empty_image, Image, unsigned char)
DEFINE_CREATE_IMAGE_FUNC(create_empty_image_s, ImageS, short)
DEFINE_CREATE_IMAGE_FUNC(create_empty_image_f, ImageF, float)

Image create_image_mask(int width, int height, float range, int left, int right)
{
    return create_mask(width, height, range, left, right);
}

int save_image(const Image *img, char *out_filename)
{
    if (img->channels == CHANNELS)
    {
        return compress_jpeg(out_filename, img, 100);
    }
    else
    {
        return compress_grayscale_jpeg(out_filename, img, 100);
    }
}

void crop_image(Image *img, int cut_top, int cut_bottom, int cut_left, int cut_right)
{
    crop_image_buf(img, cut_top, cut_bottom, cut_left, cut_right, img->channels);
}

#define DEFINE_DESTROY_IMAGE_FUNC(NAME, PIXEL_T) \
    void NAME(PIXEL_T *img)                      \
    {                                            \
        if (img->data)                           \
        {                                        \
            free(img->data);                     \
        }                                        \
    }

DEFINE_DESTROY_IMAGE_FUNC(destroy_image, Image)
DEFINE_DESTROY_IMAGE_FUNC(destroy_image_s, ImageS)
DEFINE_DESTROY_IMAGE_FUNC(destroy_image_f, ImageF)

#define DEFINE_IMAGE_SIZE_FUNC(NAME, IMAGE_T)            \
    int NAME(IMAGE_T *img)                               \
    {                                                    \
        return img->channels * img->height * img->width; \
    }

DEFINE_IMAGE_SIZE_FUNC(image_size, Image)
DEFINE_IMAGE_SIZE_FUNC(image_size_s, ImageS)
DEFINE_IMAGE_SIZE_FUNC(image_size_f, ImageF)

Rect create_rect(int x, int y, int width, int height)
{
    Rect rect;
    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;
    return rect;
}

Point br(Rect r)
{
    Point result;
    result.x = r.x + r.width;
    result.y = r.y + r.height;
    return result;
}

Blender *create_blender(Rect out_size, int nb)
{
    Blender *blender = (Blender *)malloc(sizeof(Blender));
    if (!blender)
        return NULL;

    blender->num_bands = min(MAX_BANDS, nb);

    double max_len = (double)(out_size.width > out_size.height ? out_size.width : out_size.height);
    blender->num_bands = min(blender->num_bands, (int)ceil(log(max_len) / log(2.0)));

    out_size.width += ((1 << blender->num_bands) - out_size.width % (1 << blender->num_bands)) % (1 << blender->num_bands);
    out_size.height += ((1 << blender->num_bands) - out_size.height % (1 << blender->num_bands)) % (1 << blender->num_bands);
    blender->output_size = out_size;

    blender->out = (ImageF *)malloc((blender->num_bands + 1) * sizeof(ImageF));
    blender->final_out = (Image *)malloc((blender->num_bands + 1) * sizeof(Image));
    blender->out_mask = (ImageF *)malloc((blender->num_bands + 1) * sizeof(ImageF));
    blender->out_width_levels = (int *)malloc((blender->num_bands + 1) * sizeof(int));
    blender->out_height_levels = (int *)malloc((blender->num_bands + 1) * sizeof(int));
    blender->img_laplacians = (Image *)malloc((blender->num_bands + 1) * sizeof(Image));
    blender->mask_gaussian = (Image *)malloc((blender->num_bands + 1) * sizeof(Image));

    if (!blender->out ||
        !blender->final_out ||
        !blender->out_mask ||
        !blender->out_width_levels ||
        !blender->out_height_levels ||
        !blender->img_laplacians ||
        !blender->mask_gaussian)
    {
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
    blender->out_mask[0] = create_empty_image_f(out_size.width, out_size.height, 1);

    blender->out_width_levels[0] = out_size.width;
    blender->out_height_levels[0] = out_size.height;

    for (int i = 1; i <= blender->num_bands; i++)
    {
        blender->out_width_levels[i] = (blender->out_width_levels[i - 1] + 1) / 2;
        blender->out_height_levels[i] = (blender->out_height_levels[i - 1] + 1) / 2;

        blender->out[i] = create_empty_image_f(blender->out_width_levels[i], blender->out_height_levels[i], 3);
        blender->out_mask[i] = create_empty_image_f(blender->out_width_levels[i], blender->out_height_levels[i], 1);
    }

    return blender;
}

void destroy_blender(Blender *blender)
{
    if (!blender)
        return;

    if (blender->out != NULL)
    {
        free(blender->out);
    }

    if (blender->out_mask != NULL)
    {
        free(blender->out_mask);
    }

    free(blender->out_width_levels);
    free(blender->out_height_levels);

    destroy_image(&blender->result);
    destroy_image(blender->final_out);

    free(blender->img_laplacians);
    free(blender->mask_gaussian);

    free(blender);
}

void *down_sample_operation(void *args)
{
    ThreadArgs *arg = (ThreadArgs *)args;
    int start_row = arg->start_index;
    int end_row = arg->end_index;
    SamplingThreadData *data = (SamplingThreadData *)arg->workerThreadArgs->std;
    Image *img = (Image *)data->img;

    int imageSize = image_size(data->img);
    unsigned char *sampled = (unsigned char *)data->sampled;

    for (int y = start_row; y < end_row; ++y)
    {
        for (int x = 0; x < data->new_width; ++x)
        {
            for (int c = 0; c < img->channels; ++c)
            {
                float sum = 0.0;
                for (int i = -2; i < 3; i++)
                {
                    for (int j = -2; j < 3; j++)
                    {
                        int src_row = 2 * y + i;
                        int src_col = 2 * x + j;

                        int rr = reflect_index(src_row, img->height);
                        int cc = reflect_index(src_col, img->width);

                        int pos = (cc + rr * img->width) * img->channels + c;

                        if (pos < imageSize)
                        {
                            sum += GAUSSIAN_KERNEL[i + 2][j + 2] * img->data[pos];
                        }
                    }
                }

                sampled[(y * data->new_width + x) * img->channels + c] = (unsigned char)clamp(ceil(sum), 0, 255);
            }
        }
    }

    return NULL;
}

Image downsample(Image *img)
{
    Image result;
    int new_width = img->width / 2;
    int new_height = img->height / 2;
    unsigned char *downsampled = (unsigned char *)malloc(new_width * new_height * img->channels);
    if (!downsampled)
    {
        return result;
    }

    SamplingThreadData std = {0, new_width, new_height, img, downsampled};
    WorkerThreadArgs wtd;
    wtd.std = &std;
    ParallelOperatorArgs args = {new_height, &wtd};

    parallel_operator(DOWNSAMPLE, &args);

    result.channels = img->channels;
    result.data = downsampled;
    result.width = new_width;
    result.height = new_height;
    return result;
}

#define DEFINE_UPSAMPLE_WORKER_FUNC(NAME, IMAGE_T, PIXEL_T)                                         \
    void *NAME(void *args)                                                                          \
    {                                                                                               \
        ThreadArgs *arg = (ThreadArgs *)args;                                                       \
        int start_row = arg->start_index;                                                           \
        int end_row = arg->end_index;                                                               \
        SamplingThreadData *s = (SamplingThreadData *)arg->workerThreadArgs->std;                   \
        IMAGE_T *img = (IMAGE_T *)s->img;                                                           \
        PIXEL_T *sampled = (PIXEL_T *)s->sampled;                                                   \
        int pad = 2;                                                                                \
        for (int y = start_row; y < end_row; ++y)                                                   \
        {                                                                                           \
            for (int x = 0; x < s->new_width; ++x)                                                  \
            {                                                                                       \
                for (int c = 0; c < img->channels; ++c)                                             \
                {                                                                                   \
                    float sum = 0;                                                                  \
                    for (int ki = 0; ki < 5; ki++)                                                  \
                    {                                                                               \
                        for (int kj = 0; kj < 5; kj++)                                              \
                        {                                                                           \
                            int src_i = reflect_index(y + ki - pad, s->new_height);                 \
                            int src_j = reflect_index(x + kj - pad, s->new_width);                  \
                            int pixel_val = 0;                                                      \
                            if (src_i % 2 == 0 && src_j % 2 == 0)                                   \
                            {                                                                       \
                                int orig_i = src_i / 2;                                             \
                                int orig_j = src_j / 2;                                             \
                                int image_pos = (orig_i * img->width + orig_j) * img->channels + c; \
                                pixel_val = img->data[image_pos] * s->upsample_factor;              \
                            }                                                                       \
                            sum += GAUSSIAN_KERNEL[ki][kj] * pixel_val;                             \
                        }                                                                           \
                    }                                                                               \
                    int up_image_pos = (y * s->new_width + x) * img->channels + c;                  \
                    sampled[up_image_pos] = (PIXEL_T)clamp(floor(sum + 0.5), 0, 255);               \
                }                                                                                   \
            }                                                                                       \
        }                                                                                           \
        return NULL;                                                                                \
    }

DEFINE_UPSAMPLE_WORKER_FUNC(upsample_worker, Image, unsigned char)
DEFINE_UPSAMPLE_WORKER_FUNC(upsample_worker_s, ImageS, short)
DEFINE_UPSAMPLE_WORKER_FUNC(upsample_worker_f, ImageF, float)

#define DEFINE_UPSAMPLE_FUNC(NAME, IMAGE_T, PIXEL_T)                                                     \
    IMAGE_T NAME(IMAGE_T *img, float upsample_factor)                                                    \
    {                                                                                                    \
        IMAGE_T result;                                                                                  \
        int new_width = img->width * 2;                                                                  \
        int new_height = img->height * 2;                                                                \
        PIXEL_T *upsampled = (PIXEL_T *)calloc(new_width * new_height * img->channels, sizeof(PIXEL_T)); \
        if (!upsampled)                                                                                  \
        {                                                                                                \
            result.data = NULL;                                                                          \
            result.width = result.height = result.channels = 0;                                          \
            return result;                                                                               \
        }                                                                                                \
        SamplingThreadData std = {upsample_factor, new_width, new_height, img, upsampled};               \
        WorkerThreadArgs wtd;                                                                            \
        wtd.std = &std;                                                                                  \
        ParallelOperatorArgs args = {new_height, &wtd};                                                  \
        parallel_operator(UPSAMPLE, &args);                                                              \
        result.data = upsampled;                                                                         \
        result.width = new_width;                                                                        \
        result.height = new_height;                                                                      \
        result.channels = img->channels;                                                                 \
        return result;                                                                                   \
    }

DEFINE_UPSAMPLE_FUNC(upsample, Image, unsigned char)
DEFINE_UPSAMPLE_FUNC(upsample_image_s, ImageS, short)
DEFINE_UPSAMPLE_FUNC(upsample_image_f, ImageF, float)

void *compute_laplacian_worker(void *args)
{
    ThreadArgs *arg = (ThreadArgs *)args;
    int start_row = arg->start_index;
    int end_row = arg->end_index;
    LaplacianThreadData *data = (LaplacianThreadData *)arg->workerThreadArgs->ltd;

    for (int i = start_row; i < end_row; ++i)
    {
        data->upsampled_data[i] = (unsigned char)clamp(data->original_data[i] - data->upsampled_data[i], 0, 255);
    }

    return NULL;
}

void compute_laplacian(Image *original, Image *upsampled)
{
    int total_size = original->width * original->height * original->channels;

    LaplacianThreadData ltd = {original->data, upsampled->data, total_size};
    WorkerThreadArgs wtd;
    wtd.ltd = &ltd;
    ParallelOperatorArgs args = {total_size, &wtd};

    parallel_operator(LAPLACIAN, &args);
}

void *feed_worker(void *args)
{
    ThreadArgs *arg = (ThreadArgs *)args;
    int start_row = arg->start_index;
    int end_row = arg->end_index;
    FeedThreadData *f = (FeedThreadData *)arg->workerThreadArgs->ftd;

    for (int k = start_row; k < end_row; ++k)
    {
        for (int i = 0; i < f->cols; ++i)
        {
            int maskIndex = i + (k * f->level_width);
            int outMaskLevelIndex = ((i + f->x_tl) + ((k + f->y_tl) * f->out_level_width));

            for (int z = 0; z < CHANNELS; ++z)
            {
                int imgIndex = ((i + (k * f->level_width)) * CHANNELS) + z;

                if (imgIndex < f->img_laplacians[f->level].width * f->img_laplacians[f->level].height * CHANNELS &&
                    maskIndex < f->mask_gaussian[f->level].width * f->mask_gaussian[f->level].height)
                {

                    int outLevelIndex = ((i + f->x_tl) + (k + f->y_tl) * f->out_level_width) * CHANNELS + z;

                    float maskVal = f->mask_gaussian[f->level].data[maskIndex];
                    float imgVal = f->img_laplacians[f->level].data[imgIndex];

                    maskVal = maskVal * (1.0 / 255.0);

                    imgVal = imgVal * maskVal;

                    if (outLevelIndex < f->out_level_height * f->out_level_width * CHANNELS &&
                        outMaskLevelIndex < f->out_level_height * f->out_level_width)
                    {
                        f->out[f->level].data[outLevelIndex] += imgVal;

                        if (z == 0)
                        {
                            f->out_mask[f->level].data[outMaskLevelIndex] += maskVal;
                        }
                    }
                }
            }
        }
    }

    return NULL;
}

int feed(Blender *b, Image *img, Image *mask_img, Point tl)
{
    char buf[100];
    int return_val = 1;

    int gap = 3 * (1 << b->num_bands);
    Point tl_new, br_new;

    tl_new.x = max(b->output_size.x, tl.x - gap);
    tl_new.y = max(b->output_size.y, tl.y - gap);

    Point br_point = br(b->output_size);
    br_new.x = min(br_point.x, tl.x + img->width + gap);
    br_new.y = min(br_point.y, tl.y + img->height + gap);

    tl_new.x = b->output_size.x + (((tl_new.x - b->output_size.x) >> b->num_bands) << b->num_bands);
    tl_new.y = b->output_size.y + (((tl_new.y - b->output_size.y) >> b->num_bands) << b->num_bands);

    int width = br_new.x - tl_new.x;
    int height = br_new.y - tl_new.y;

    width += ((1 << b->num_bands) - width % (1 << b->num_bands)) % (1 << b->num_bands);
    height += ((1 << b->num_bands) - height % (1 << b->num_bands)) % (1 << b->num_bands);

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

    add_border_to_image(img, top, bottom, left, right, CHANNELS, BORDER_REFLECT);
    add_border_to_image(mask_img, top, bottom, left, right, 1, BORDER_CONSTANT);

    Image images[b->num_bands + 1];
    images[0] = *img;
    for (int j = 0; j < b->num_bands; ++j)
    {

        images[j + 1] = downsample(&images[j]);
        if (!images[j + 1].data)
        {
            return_val = 0;
            goto clean;
        }

        Image up = upsample(&images[j + 1], 4.f);
        if (!up.data)
        {
            return_val = 0;
            goto clean;
        }

        compute_laplacian(&images[j], &up);
        b->img_laplacians[j] = up;
    }

    b->img_laplacians[b->num_bands] = images[b->num_bands];
    Image sampled;
    for (int j = 0; j < b->num_bands; ++j)
    {
        b->mask_gaussian[j] = *mask_img;
        sampled = downsample(mask_img);
        if (!sampled.data)
        {
            return_val = 0;
            goto clean;
        }
        mask_img = &sampled;
    }

    b->mask_gaussian[b->num_bands] = *mask_img;

    int y_tl = tl_new.y - b->output_size.y;
    int y_br = br_new.y - b->output_size.y;
    int x_tl = tl_new.x - b->output_size.x;
    int x_br = br_new.x - b->output_size.x;

    for (int level = 0; level <= b->num_bands; ++level)
    {

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
    return return_val;
}

void *blend_worker(void *args)
{
    ThreadArgs *arg = (ThreadArgs *)args;
    int start_row = arg->start_index;
    int end_row = arg->end_index;
    BlendThreadData *b = (BlendThreadData *)arg->workerThreadArgs->btd;
    for (int i = start_row; i < end_row; ++i)
    {
        if (i < b->out_size)
        {
            b->blended_image.data[i] = clamp(
                b->blended_image.data[i] + b->out_level.data[i], 0, 255);
        }
    }
    return NULL;
}

void *normalize_worker(void *args)
{
    ThreadArgs *arg = (ThreadArgs *)args;
    int start_row = arg->start_index;
    int end_row = arg->end_index;
    NormalThreadData *n = (NormalThreadData *)arg->workerThreadArgs->ntd;

    for (int y = start_row; y < end_row; ++y)
    {
        for (int x = 0; x < n->output_width; ++x)
        {
            int maskIndex = x + (y * n->output_width);
            if (maskIndex < image_size_f(&n->out_mask[n->level]))
            {
                float w = n->out_mask[n->level].data[maskIndex];

                for (int z = 0; z < CHANNELS; z++)
                {
                    int imgIndex = (x + (y * n->output_width)) * CHANNELS + z;
                    if (imgIndex < image_size(&n->final_out[n->level]))
                    {

                        n->final_out[n->level].data[imgIndex] = (n->out[n->level].data[imgIndex] / (w + WEIGHT_EPS));
                    }
                }
            }
        }
    }
    return NULL;
}

void blend(Blender *b)
{

    char buf[100];

    for (int level = 0; level <= b->num_bands; ++level)
    {
        b->final_out[level] = create_empty_image(b->out[level].width, b->out[level].height, b->out[level].channels);

        NormalThreadData ntd = {b->out[level].width, level, b->out, b->out_mask, b->final_out};
        WorkerThreadArgs wtd;
        wtd.ntd = &ntd;
        ParallelOperatorArgs args = {b->out[level].height, &wtd};

        parallel_operator(NORMALIZE, &args);
        destroy_image_f(&b->out[level]);
        destroy_image_f(&b->out_mask[level]);
    }

    Image blended_image = b->final_out[b->num_bands];
    Image ubi;

    for (int level = b->num_bands; level > 0; --level)
    {
        ubi = upsample(&blended_image, 4.f);
        blended_image = ubi;
        int out_size = image_size(&b->final_out[level - 1]);

        BlendThreadData btd = {out_size, blended_image, b->final_out[level - 1]};
        WorkerThreadArgs wtd;
        wtd.btd = &btd;
        ParallelOperatorArgs args = {out_size, &wtd};
        parallel_operator(BLEND, &args);
    }

    b->result.data = (unsigned char *)malloc(b->output_size.width * b->output_size.height * CHANNELS * sizeof(unsigned char));
    b->result.channels = blended_image.channels;
    b->result.width = blended_image.width;
    b->result.height = blended_image.height;
    memcpy(b->result.data, blended_image.data, image_size(&blended_image));
    free(ubi.data);
}

void parallel_operator(OperatorType operatorType, ParallelOperatorArgs *arg)
{
    int numThreads = get_cpus_count();
    if (operatorType == FEED || operatorType == NORMALIZE)
    {
        numThreads = 1;
    }

    int rowsPerThread = arg->rows / numThreads;
    int remainingRows = arg->rows % numThreads;

    pthread_t threads[numThreads];
    ThreadArgs thread_data[numThreads];

    int startRow = 0;
    for (unsigned int i = 0; i < numThreads; ++i)
    {
        int endRow = startRow + rowsPerThread + (remainingRows > 0 ? 1 : 0);
        if (remainingRows > 0)
        {
            --remainingRows;
        }

        switch (operatorType)
        {
        case UPSAMPLE:
        case DOWNSAMPLE:
            thread_data[i].end_index = endRow;
            thread_data[i].start_index = startRow;
            thread_data[i].workerThreadArgs = arg->workerThreadArgs;
            if (operatorType == DOWNSAMPLE)
            {
                pthread_create(&threads[i], NULL, down_sample_operation, &thread_data[i]);
            }
            else
            {
                pthread_create(&threads[i], NULL, upsample_worker, &thread_data[i]);
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
            pthread_create(&threads[i], NULL, compute_laplacian_worker, &thread_data[i]);
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

    for (unsigned int i = 0; i < numThreads; ++i)
    {
        pthread_join(threads[i], NULL);
    }
}
