#ifndef DEBUG
#define DEBUG 0
#endif

#include "turbojpeg.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "utils.h"
#include "laplace_blending.h"

Image create_image(const char *filename)
{
    return decompress_jpeg(filename);
}

Image create_empty_image(int width, int height, int channels)
{
    Image img;

    img.data = (unsigned char *)calloc(width * height * channels, sizeof(unsigned char));
    if (!img.data)
    {
        return img;
    }
    img.channels = channels;
    img.width = width;
    img.height = height;

    return img;
}

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

void destroy_image(Image *img)
{
    if (img->data)
    {
        free(img->data);
    }
}

int image_size(Image *img)
{
    return img->channels * img->height * img->width;
}

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

    blender->num_bands = nb;

    double max_len = (double)(out_size.width > out_size.height ? out_size.width : out_size.height);
    blender->num_bands = fmin(blender->num_bands, (int)ceil(log(max_len) / log(2.0)));

    out_size.width += ((1 << blender->num_bands) - out_size.width % (1 << blender->num_bands)) % (1 << blender->num_bands);
    out_size.height += ((1 << blender->num_bands) - out_size.height % (1 << blender->num_bands)) % (1 << blender->num_bands);
    blender->output_size = out_size;

    blender->out = (Image *)malloc((blender->num_bands + 1) * sizeof(Image));
    blender->out_mask = (Image *)malloc((blender->num_bands + 1) * sizeof(Image));
    blender->out_width_levels = (int *)malloc((blender->num_bands + 1) * sizeof(int));
    blender->out_height_levels = (int *)malloc((blender->num_bands + 1) * sizeof(int));
    blender->img_laplacians = (Image *)malloc((blender->num_bands + 1) * sizeof(Image));
    blender->mask_gaussian = (Image *)malloc((blender->num_bands + 1) * sizeof(Image));

    if (!blender->out ||
        !blender->out_mask ||
        !blender->out_width_levels ||
        !blender->out_height_levels ||
        !blender->img_laplacians ||
        !blender->mask_gaussian)
    {
        free(blender->out);
        free(blender->out_mask);
        free(blender->out_width_levels);
        free(blender->out_height_levels);
        free(blender->img_laplacians);
        free(blender->mask_gaussian);
        free(blender);
        return NULL;
    }

    blender->out[0] = create_empty_image(out_size.width, out_size.height, 3);
    blender->out_mask[0] = create_empty_image(out_size.width, out_size.height, 1);

    blender->out_width_levels[0] = out_size.width;
    blender->out_height_levels[0] = out_size.height;

    for (int i = 1; i <= blender->num_bands; i++)
    {
        blender->out_width_levels[i] = (blender->out_width_levels[i - 1] + 1) / 2;
        blender->out_height_levels[i] = (blender->out_height_levels[i - 1] + 1) / 2;

        blender->out[i] = create_empty_image(blender->out_width_levels[i], blender->out_height_levels[i], 3);
        blender->out_mask[i] = create_empty_image(blender->out_width_levels[i], blender->out_height_levels[i], 1);
    }

    return blender;
}

void destroy_blender(Blender *blender)
{
    if (!blender)
        return;

    for (int i = 0; i <= blender->num_bands; ++i)
    {
        destroy_image(&blender->out[i]);
        destroy_image(&blender->out_mask[i]);
    }

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
    for (int y = start_row; y < end_row; ++y)
    {
        for (int x = 0; x < data->new_width; ++x)
        {
            for (int c = 0; c < data->img->channels; ++c)
            {
                int sum = 0;
                sum += data->img->data[(2 * y * data->img->width + 2 * x) * data->img->channels + c];
                sum += data->img->data[(2 * y * data->img->width + 2 * x + 1) * data->img->channels + c];
                sum += data->img->data[((2 * y + 1) * data->img->width + 2 * x) * data->img->channels + c];
                sum += data->img->data[((2 * y + 1) * data->img->width + 2 * x + 1) * data->img->channels + c];

                data->sampled[(y * data->new_width + x) * data->img->channels + c] = sum >> 4;
            }
        }
    }
    return NULL;
}

void *maskdown_sample_operation(void *args)
{
    ThreadArgs *arg = (ThreadArgs *)args;
    int start_row = arg->start_index;
    int end_row = arg->end_index;
    SamplingThreadData *data = (SamplingThreadData *)arg->workerThreadArgs->std;
    for (int y = start_row; y < end_row; ++y)
    {
        for (int x = 0; x < data->new_width; ++x)
        {
            data->sampled[y * data->new_width + x] = data->img->data[(2 * y) * data->img->width + ((2 * x) + 1)];
        }
    }
    return NULL;
}

Image downsample(const Image *img)
{
    Image result;
    int new_width = img->width / 2;
    int new_height = img->height / 2;
    unsigned char *downsampled = (unsigned char *)malloc(new_width * new_height * img->channels);
    if (!downsampled)
    {
        return result;
    }

    SamplingThreadData std = {new_width, new_height, img, downsampled};
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

void *upsample_worker(void *args)
{
    ThreadArgs *arg = (ThreadArgs *)args;
    int start_row = arg->start_index;
    int end_row = arg->end_index;
    SamplingThreadData *s = (SamplingThreadData *)arg->workerThreadArgs->std;

    for (int y = start_row; y < end_row; ++y)
    {
        for (int x = 0; x < s->new_width; ++x)
        {
            for (int c = 0; c < s->img->channels; ++c)
            {
                float srcX = x / 2.0f;
                float srcY = y / 2.0f;

                int x0 = (int)srcX;
                int y0 = (int)srcY;
                float fx = srcX - x0;
                float fy = srcY - y0;

                if (x0 >= s->img->width - 1)
                    x0 = s->img->width - 2;
                if (y0 >= s->img->height - 1)
                    y0 = s->img->height - 2;

                unsigned char p00 = s->img->data[(y0 * s->img->width + x0) * s->img->channels + c];
                unsigned char p01 = s->img->data[(y0 * s->img->width + (x0 + 1)) * s->img->channels + c];
                unsigned char p10 = s->img->data[((y0 + 1) * s->img->width + x0) * s->img->channels + c];
                unsigned char p11 = s->img->data[((y0 + 1) * s->img->width + (x0 + 1)) * s->img->channels + c];

                unsigned char interpolated = (unsigned char)((1 - fx) * (1 - fy) * p00 +
                                                             fx * (1 - fy) * p01 +
                                                             (1 - fx) * fy * p10 +
                                                             fx * fy * p11);

                s->sampled[(y * s->new_width + x) * s->img->channels + c] = interpolated;
            }
        }
    }

    return NULL;
}

Image upsample(const Image *img)
{
    Image result;
    int new_width = img->width * 2;
    int new_height = img->height * 2;
    unsigned char *upsampled = (unsigned char *)malloc(new_width * new_height * img->channels);
    if (!upsampled)
    {
        return result;
    }
    SamplingThreadData std = {new_width, new_height, img, upsampled};
    WorkerThreadArgs wtd;
    wtd.std = &std;
    ParallelOperatorArgs args = {new_height, &wtd};

    parallel_operator(UPSAMPLE, &args);

    result.data = upsampled;
    result.width = new_width;
    result.height = new_height;
    result.channels = img->channels;

    return result;
}

void *compute_laplacian_worker(void *args)
{
    ThreadArgs *arg = (ThreadArgs *)args;
    int start_row = arg->start_index;
    int end_row = arg->end_index;
    LaplacianThreadData *data = (LaplacianThreadData *)arg->workerThreadArgs->ltd;

    for (int i = start_row; i < end_row; ++i)
    {
        data->laplacian_data[i] = (unsigned char)clamp(data->original_data[i] - data->upsampled_data[i], 1, 256) - 1;
    }

    return NULL;
}

Image compute_laplacian(Image *original, Image *upsampled)
{
    Image result;
    int total_size = original->width * original->height * original->channels;
    unsigned char *laplacian = (unsigned char *)malloc(total_size);
    if (!laplacian)
    {
        return result;
    }

    LaplacianThreadData ltd = {original->data, upsampled->data, laplacian, total_size};
    WorkerThreadArgs wtd;
    wtd.ltd = &ltd;
    ParallelOperatorArgs args = {total_size, &wtd};

    parallel_operator(LAPLACIAN, &args);

    result.data = laplacian;
    result.width = original->width;
    result.height = original->height;
    result.channels = original->channels;

    return result;
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
            for (int z = 0; z < CHANNELS; ++z)
            {
                int imgIndex = ((i + (k * f->level_width)) * CHANNELS) + z;
                int maskIndex = imgIndex / CHANNELS;

                if (imgIndex < f->img_laplacians[f->level].width * f->img_laplacians[f->level].height * CHANNELS &&
                    maskIndex < f->mask_gaussian[f->level].width * f->mask_gaussian[f->level].height)
                {

                    int outLevelIndex = (((i + f->x_tl) + ((k + f->y_tl) * f->out_level_width)) * CHANNELS) + z;
                    int outMaskLevelIndex = ((i + f->x_tl) + ((k + f->y_tl) * f->out_level_width));

                    int imgVal = f->img_laplacians[f->level].data[imgIndex];
                    int maskVal = (f->mask_gaussian[f->level].data[maskIndex] + 1) >> 8;

                    if (outLevelIndex < f->out_level_height * f->out_level_width * CHANNELS)
                    {
                        if (maskVal >= 1)
                        {

                            if (z == 0)
                            {
                                f->out_mask[f->level].data[outMaskLevelIndex] += maskVal;
                            }

                            int oldVal = f->out[f->level].data[outLevelIndex];
                            int avg = oldVal + ((imgVal - oldVal) / f->out_mask[f->level].data[outMaskLevelIndex]);

                            f->out[f->level].data[outLevelIndex] = avg;
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
    int return_val = 1;
    int num_threads = get_cpus_count();

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

        // char buf[100];
        // sprintf(buf,"image%d.jpg",j);
        // if (save_image(&images[j + 1], buf))
        // {
        //     printf("Merged image  saved \n");
        // }

        Image up = upsample(&images[j + 1]);
        if (!up.data)
        {
            return_val = 0;
            goto clean;
        }

        b->img_laplacians[j] = compute_laplacian(&images[j], &up);
        destroy_image(&up);
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

void blend(Blender *b)
{
    clock_t start = clock();

    Image blended_image = b->out[b->num_bands];
    Image ubi;

    for (int level = b->num_bands; level > 0; --level)
    {
        ubi = upsample(&blended_image);
        blended_image = ubi;
        int out_size = image_size(&b->out[level - 1]);

        BlendThreadData btd = {out_size, blended_image, b->out[level - 1]};
        WorkerThreadArgs wtd;
        wtd.btd = &btd;
        ParallelOperatorArgs args = {out_size, &wtd};
        parallel_operator(BLEND, &args);
    }

    b->result.data = (unsigned char *)malloc(image_size(&blended_image));
    b->result.channels = blended_image.channels;
    b->result.width = blended_image.width;
    b->result.height = blended_image.height;
    memmove(b->result.data, blended_image.data, image_size(&blended_image));

    free(ubi.data);

    if (DEBUG)
    {
        clock_t end = clock();
        double duration = (double)(end - start) / CLOCKS_PER_SEC;
        printf("Elapsed time for blend: %.2f seconds\n", duration);
    }
}


void parallel_operator(OperatorType operatorType, ParallelOperatorArgs *arg)
{
    int num_threads = get_cpus_count();

    int rowsPerThread = arg->rows / num_threads;
    int remainingRows = arg->rows % num_threads;

    pthread_t threads[num_threads];
    ThreadArgs thread_data[num_threads];

    int start_row = 0;
    for (unsigned int i = 0; i < num_threads; ++i)
    {
        int end_row = start_row + rowsPerThread + (remainingRows > 0 ? 1 : 0);
        if (remainingRows > 0)
        {
            --remainingRows;
        }

        switch (operatorType)
        {
        case UPSAMPLE:
        case DOWNSAMPLE:
            thread_data[i].end_index = end_row;
            thread_data[i].start_index = start_row;
            thread_data[i].workerThreadArgs = arg->workerThreadArgs;
            if (operatorType == DOWNSAMPLE)
            {
                if (arg->workerThreadArgs->std->img->channels == 3)
                {
                    pthread_create(&threads[i], NULL, down_sample_operation, &thread_data[i]);
                }
                else
                {
                    pthread_create(&threads[i], NULL, maskdown_sample_operation, &thread_data[i]);
                }
            }
            else
            {
                pthread_create(&threads[i], NULL, upsample_worker, &thread_data[i]);
            }

            break;
        case FEED:
            thread_data[i].end_index = end_row;
            thread_data[i].start_index = start_row;
            thread_data[i].workerThreadArgs = arg->workerThreadArgs;
            pthread_create(&threads[i], NULL, feed_worker, &thread_data[i]);
            break;
        case LAPLACIAN:
            thread_data[i].end_index = end_row;
            thread_data[i].start_index = start_row;
            thread_data[i].workerThreadArgs = arg->workerThreadArgs;
            pthread_create(&threads[i], NULL, compute_laplacian_worker, &thread_data[i]);
            break;
        case BLEND:
            thread_data[i].end_index = end_row;
            thread_data[i].start_index = start_row;
            thread_data[i].workerThreadArgs = arg->workerThreadArgs;
            pthread_create(&threads[i], NULL, blend_worker, &thread_data[i]);
            break;
        }

        start_row = end_row;
    }

    for (unsigned int i = 0; i < num_threads; ++i)
    {
        pthread_join(threads[i], NULL);
    }
}