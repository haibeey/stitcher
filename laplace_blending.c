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

void *down_sample_operation(void *arg)
{
    SamplingThreadData *data = (SamplingThreadData *)arg;
    for (int y = data->start_row; y < data->end_row; ++y)
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

void *maskdown_sample_operation(void *arg)
{
    SamplingThreadData *data = (SamplingThreadData *)arg;
    for (int y = data->start_row; y < data->end_row; ++y)
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

    int num_threads = get_cpus_count();

    int rowsPerThread = new_height / num_threads;
    int remainingRows = new_height % num_threads;

    pthread_t threads[num_threads];
    SamplingThreadData thread_data[num_threads];

    int start_row = 0;
    for (unsigned int i = 0; i < num_threads; ++i)
    {
        int end_row = start_row + rowsPerThread + (remainingRows > 0 ? 1 : 0);
        if (remainingRows > 0)
        {
            --remainingRows;
        }

        thread_data[i].start_row = start_row;
        thread_data[i].end_row = end_row;
        thread_data[i].new_width = new_width;
        thread_data[i].new_height = new_height;
        thread_data[i].img = img;
        thread_data[i].sampled = downsampled;

        if (img->channels == 3)
        {
            pthread_create(&threads[i], NULL, down_sample_operation, &thread_data[i]);
        }
        else
        {
            pthread_create(&threads[i], NULL, maskdown_sample_operation, &thread_data[i]);
        }

        start_row = end_row;
    }

    for (unsigned int i = 0; i < num_threads; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    result.channels = img->channels;
    result.data = downsampled;
    result.width = new_width;
    result.height = new_height;
    return result;
}

void *upsample_worker(void *args)
{
    SamplingThreadData *s = (SamplingThreadData *)args;

    for (int y = s->start_row; y < s->end_row; ++y)
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

    int num_threads = get_cpus_count();

    pthread_t threads[num_threads];
    SamplingThreadData thread_data[num_threads];

    int rowsPerThread = new_height / num_threads;
    int remainingRows = new_height % num_threads;

    for (int i = 0; i < num_threads; ++i)
    {
        thread_data[i].img = img;
        thread_data[i].sampled = upsampled;
        thread_data[i].new_width = new_width;
        thread_data[i].new_height = new_height;
        thread_data[i].start_row = i * rowsPerThread;
        thread_data[i].end_row = thread_data[i].start_row + rowsPerThread + (i == num_threads - 1 ? remainingRows : 0);

        pthread_create(&threads[i], NULL, upsample_worker, &thread_data[i]);
    }

    for (int i = 0; i < num_threads; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    result.data = upsampled;
    result.width = new_width;
    result.height = new_height;
    result.channels = img->channels;

    return result;
}

void *compute_laplacian_worker(void *arg)
{
    LaplacianWorkerArgs *args = (LaplacianWorkerArgs *)arg;

    for (int i = args->start_index; i < args->end_index; ++i)
    {
        args->laplacian_data[i] = (unsigned char)clamp(args->original_data[i] - args->upsampled_data[i], 1, 256) - 1;
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

    int num_threads = get_cpus_count();

    pthread_t threads[num_threads];
    LaplacianWorkerArgs thread_data[num_threads];

    int elementsPerThread = total_size / num_threads;
    int remainingElements = total_size % num_threads;

    for (int i = 0; i < num_threads; ++i)
    {
        thread_data[i].original_data = original->data;
        thread_data[i].upsampled_data = upsampled->data;
        thread_data[i].laplacian_data = laplacian;
        thread_data[i].total_size = total_size;
        thread_data[i].start_index = i * elementsPerThread;
        thread_data[i].end_index = thread_data[i].start_index + elementsPerThread + (i == num_threads - 1 ? remainingElements : 0);

        pthread_create(&threads[i], NULL, compute_laplacian_worker, &thread_data[i]);
    }

    for (int i = 0; i < num_threads; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    result.data = laplacian;
    result.width = original->width;
    result.height = original->height;
    result.channels = original->channels;

    return result;
}

void *feed_worker(void *args)
{
    FeedWorkerArgs *f = (FeedWorkerArgs *)args;

    for (int k = f->start_row; k < f->end_row; ++k)
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

    for (int j = 0; j < b->num_bands; ++j)
    {
        b->mask_gaussian[j] = *mask_img;
        Image sampled = downsample(mask_img);
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

        pthread_t threads[num_threads];
        FeedWorkerArgs thread_data[num_threads];

        int rows = (y_br - y_tl);
        int cols = (x_br - x_tl);

        int rowsPerThread = rows / num_threads;
        int remainingRows = rows % num_threads;

        for (int i = 0; i < num_threads; ++i)
        {
            thread_data[i].start_row = i * rowsPerThread;
            thread_data[i].end_row = (i * rowsPerThread) + rowsPerThread + (i == num_threads - 1 ? remainingRows : 0);
            thread_data[i].rows = rows;
            thread_data[i].cols = cols;
            thread_data[i].x_tl = x_tl;
            thread_data[i].y_tl = y_tl;
            thread_data[i].out_level_width = b->out_width_levels[level];
            thread_data[i].out_level_height = b->out_height_levels[level];
            thread_data[i].level_width = b->img_laplacians[level].width;
            thread_data[i].level_height = b->img_laplacians[level].height;
            thread_data[i].level = level;
            thread_data[i].img_laplacians = b->img_laplacians;
            thread_data[i].mask_gaussian = b->mask_gaussian;
            thread_data[i].out = b->out;
            thread_data[i].out_mask = b->out_mask;
            pthread_create(&threads[i], NULL, feed_worker, &thread_data[i]);
        }

        for (int i = 0; i < num_threads; ++i)
        {
            pthread_join(threads[i], NULL);
        }

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
    BlendWorkerArgs *b = (BlendWorkerArgs *)args;
    for (int i = b->start_index; i < b->end_index; ++i)
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

    int num_threads = get_cpus_count();

    Image blended_image = b->out[b->num_bands];
    Image ubi;

    for (int level = b->num_bands; level > 0; --level)
    {

        ubi = upsample(&blended_image);
        blended_image = ubi;

        int out_size = image_size(&b->out[level - 1]);
        pthread_t threads[num_threads];
        BlendWorkerArgs threadArgs[num_threads];

        int elementsPerThread = out_size / num_threads;
        int remainingElements = out_size % num_threads;

        for (int i = 0; i < num_threads; ++i)
        {
            threadArgs[i].start_index = i * elementsPerThread;
            threadArgs[i].end_index = threadArgs[i].start_index + elementsPerThread + (i == num_threads - 1 ? remainingElements : 0);
            threadArgs[i].out_size = out_size;
            threadArgs[i].blended_image = blended_image;
            threadArgs[i].out_level = b->out[level - 1];

            pthread_create(&threads[i], NULL, blend_worker, &threadArgs[i]);
        }

        for (int i = 0; i < num_threads; ++i)
        {
            pthread_join(threads[i], NULL);
        }
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
