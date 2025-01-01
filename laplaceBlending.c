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
#include "laplaceBlending.h"
#include "edJpeg.h"

const int CHANNELS = 3;

Image *create_image(const char *filename)
{
    Image *img = (Image *)malloc(sizeof(Image));
    if (!img)
        return NULL;

    if (decompress_jpeg(filename, &img->data, &img->width, &img->height) <= 0)
    {
        free(img);
        return NULL;
    }
    img->channels = CHANNELS;

    return img;
}

Image *create_empty_image(int width, int height, int channels)
{
    Image *img = (Image *)malloc(sizeof(Image));
    if (!img)
        return NULL;

    img->data = (unsigned char *)malloc(width * height * channels);
    memset(img->data, 0, width * height * channels * sizeof(unsigned char));
    img->channels = channels;
    img->width = width;
    img->height = height;

    return img;
}

Image *create_image_mask(int width, int height, float range, int left, int right)
{
    Image *img = (Image *)malloc(sizeof(Image));
    if (!img)
        return NULL;
    img->width = width;
    img->height = height;

    img->data = create_mask(width, height, range, left, right);

    img->channels = 1;
    return img;
}

int save_image(Image *img, char *out_filename)
{
    if (img->channels == CHANNELS)
    {
        return compress_jpeg(out_filename, img->data, img->width, img->height, 100);
    }
    else
    {
        return compress_grayscale_jpeg(out_filename, img->data, img->width, img->height, 100);
    }
}

void crop_image(Image *img, int cut_top, int cut_bottom, int cut_left, int cut_right)
{
    crop_image_buf(&img->data, &img->width, &img->height, cut_top, cut_bottom, cut_left, cut_right, img->channels);
}

void destroy_image(Image *img)
{
    if (img)
    {
        free(img->data);
        free(img);
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

    blender->out = (Image **)malloc((blender->num_bands + 1) * sizeof(Image *));
    blender->out_mask = (Image **)malloc((blender->num_bands + 1) * sizeof(Image *));
    blender->out_width_levels = (int *)malloc((blender->num_bands + 1) * sizeof(int));
    blender->out_height_levels = (int *)malloc((blender->num_bands + 1) * sizeof(int));

    if (!blender->out || !blender->out_mask || !blender->out_width_levels || !blender->out_height_levels)
    {
        free(blender->out);
        free(blender->out_mask);
        free(blender->out_width_levels);
        free(blender->out_height_levels);
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
        if (blender->out[i] != NULL)
        {
            destroy_image(blender->out[i]);
        }

        if (blender->out_mask[i] != NULL)
        {
            destroy_image(blender->out_mask[i]);
        }
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
    if (blender->result != NULL)
    {
        destroy_image(blender->result);
    }

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

                data->sampled[(y * data->new_width + x) * data->img->channels + c] = sum / 4;
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

Image downsample(Image *img)
{
    int new_width = img->width / 2;
    int new_height = img->height / 2;
    unsigned char *downsampled = (unsigned char *)malloc(new_width * new_height * img->channels);

    if (!downsampled)
    {
        exit(EXIT_FAILURE);
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

    Image result = {downsampled, new_width, new_height, img->channels};
    return result;
}

void *upsample_worker(void *arg)
{
    SamplingThreadData *args = (SamplingThreadData *)arg;

    for (int y = args->start_row; y < args->end_row; ++y)
    {
        for (int x = 0; x < args->new_width; ++x)
        {
            for (int c = 0; c < args->img->channels; ++c)
            {
                float srcX = x / 2.0f;
                float srcY = y / 2.0f;

                int x0 = (int)srcX;
                int y0 = (int)srcY;
                float fx = srcX - x0;
                float fy = srcY - y0;

                if (x0 >= args->img->width - 1)
                    x0 = args->img->width - 2;
                if (y0 >= args->img->height - 1)
                    y0 = args->img->height - 2;

                unsigned char p00 = args->img->data[(y0 * args->img->width + x0) * args->img->channels + c];
                unsigned char p01 = args->img->data[(y0 * args->img->width + (x0 + 1)) * args->img->channels + c];
                unsigned char p10 = args->img->data[((y0 + 1) * args->img->width + x0) * args->img->channels + c];
                unsigned char p11 = args->img->data[((y0 + 1) * args->img->width + (x0 + 1)) * args->img->channels + c];

                unsigned char interpolated = (unsigned char)((1 - fx) * (1 - fy) * p00 +
                                                             fx * (1 - fy) * p01 +
                                                             (1 - fx) * fy * p10 +
                                                             fx * fy * p11);

                args->sampled[(y * args->new_width + x) * args->img->channels + c] = interpolated;
            }
        }
    }

    return NULL;
}

Image upsample(Image *img)
{
    int new_width = img->width * 2;
    int new_height = img->height * 2;
    unsigned char *upsampled = (unsigned char *)malloc(new_width * new_height * img->channels);

    int num_threads = get_cpus_count();

    pthread_t *threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    SamplingThreadData *args = (SamplingThreadData *)malloc(num_threads * sizeof(SamplingThreadData));

    int rowsPerThread = new_height / num_threads;
    int remainingRows = new_height % num_threads;

    for (int i = 0; i < num_threads; ++i)
    {
        args[i].img = img;
        args[i].sampled = upsampled;
        args[i].new_width = new_width;
        args[i].new_height = new_height;
        args[i].start_row = i * rowsPerThread;
        args[i].end_row = args[i].start_row + rowsPerThread + (i == num_threads - 1 ? remainingRows : 0);

        pthread_create(&threads[i], NULL, upsample_worker, &args[i]);
    }

    for (int i = 0; i < num_threads; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(args);

    Image result;
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
        args->laplacian_data[i] = (unsigned char)clamp(args->original_data[i] - args->upsampled_data[i], 0, 255);
    }

    return NULL;
}

Image compute_laplacian(Image *original, Image *upsampled)
{
    int total_size = original->width * original->height * original->channels;
    unsigned char *laplacian = (unsigned char *)malloc(total_size);

    int num_threads = get_cpus_count();

    pthread_t *threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    LaplacianWorkerArgs *args = (LaplacianWorkerArgs *)malloc(num_threads * sizeof(LaplacianWorkerArgs));

    int elementsPerThread = total_size / num_threads;
    int remainingElements = total_size % num_threads;

    for (int i = 0; i < num_threads; ++i)
    {
        args[i].original_data = original->data;
        args[i].upsampled_data = upsampled->data;
        args[i].laplacian_data = laplacian;
        args[i].total_size = total_size;
        args[i].start_index = i * elementsPerThread;
        args[i].end_index = args[i].start_index + elementsPerThread + (i == num_threads - 1 ? remainingElements : 0);

        pthread_create(&threads[i], NULL, compute_laplacian_worker, &args[i]);
    }

    for (int i = 0; i < num_threads; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(args);

    Image result;
    result.data = laplacian;
    result.width = original->width;
    result.height = original->height;
    result.channels = original->channels;

    return result;
}

void *feed_worker(void *args)
{
    FeedWorkerArgs *wArgs = (FeedWorkerArgs *)args;

    for (int k = wArgs->start_row; k < wArgs->end_row; ++k)
    {
        for (int i = 0; i < wArgs->cols; ++i)
        {
            for (int z = 0; z < CHANNELS; ++z)
            {
                int imgIndex = ((i + (k * wArgs->level_width)) * CHANNELS) + z;
                int maskIndex = imgIndex / CHANNELS;

                if (imgIndex < wArgs->img_laplacians[wArgs->level].width * wArgs->img_laplacians[wArgs->level].height * CHANNELS &&
                    maskIndex < wArgs->mask_gaussian[wArgs->level].width * wArgs->mask_gaussian[wArgs->level].height)
                {

                    int outLevelIndex = (((i + wArgs->x_tl) + ((k + wArgs->y_tl) * wArgs->out_level_width)) * CHANNELS) + z;
                    int out_maskLevelIndex = ((i + wArgs->x_tl) + ((k + wArgs->y_tl) * wArgs->out_level_width));

                    int imgVal = wArgs->img_laplacians[wArgs->level].data[imgIndex];
                    int maskVal = wArgs->mask_gaussian[wArgs->level].data[maskIndex] / 255;

                    if (outLevelIndex < wArgs->out_level_height * wArgs->out_level_width * CHANNELS)
                    {
                        wArgs->out[wArgs->level]->data[outLevelIndex] += (imgVal * maskVal);
                        wArgs->out_mask[wArgs->level]->data[out_maskLevelIndex] += wArgs->mask_gaussian[wArgs->level].data[maskIndex];
                    }
                }
            }
        }
    }
    return NULL;
}

void feed(Blender *b, Image *img, Image *mask_img, Point tl)
{
    int num_threads = get_cpus_count();

    int gap = 3 * (1 << b->num_bands);
    Point tl_new, br_new;

    tl_new.x = (b->output_size.x > (tl.x - gap)) ? b->output_size.x : (tl.x - gap);
    tl_new.y = (b->output_size.y > (tl.y - gap)) ? b->output_size.y : (tl.y - gap);

    Point br_point = br(b->output_size);
    br_new.x = (br_point.x < (tl.x + img->width + gap)) ? br_point.x : (tl.x + img->width + gap);
    br_new.y = (br_point.y < (tl.y + img->height + gap)) ? br_point.y : (tl.y + img->height + gap);

    tl_new.x = b->output_size.x + (((tl_new.x - b->output_size.x) >> b->num_bands) << b->num_bands);
    tl_new.y = b->output_size.y + (((tl_new.y - b->output_size.y) >> b->num_bands) << b->num_bands);

    int width = br_new.x - tl_new.x;
    int height = br_new.y - tl_new.y;

    width += ((1 << b->num_bands) - width % (1 << b->num_bands)) % (1 << b->num_bands);
    height += ((1 << b->num_bands) - height % (1 << b->num_bands)) % (1 << b->num_bands);

    br_new.x = tl_new.x + width;
    br_new.y = tl_new.y + height;

    int dx = (br_new.x - br_point.x > 0) ? (br_new.x - br_point.x) : 0;
    int dy = (br_new.y - br_point.y > 0) ? (br_new.y - br_point.y) : 0;

    tl_new.x -= dx;
    br_new.x -= dx;
    tl_new.y -= dy;
    br_new.y -= dy;

    int top = tl.y - tl_new.y;
    int left = tl.x - tl_new.x;
    int bottom = br_new.y - tl.y - img->height;
    int right = br_new.x - tl.x - img->width;

    add_border_to_image(&img->data, &img->width, &img->height, top, bottom, left, right, CHANNELS, BORDER_REFLECT);
    add_border_to_image(&mask_img->data, &mask_img->width, &mask_img->height, top, bottom, left, right, 1, BORDER_CONSTANT);

    Image *img_laplacians = (Image *)malloc((b->num_bands + 1) * sizeof(Image));
    Image *current_img = img;

    for (int j = 0; j < b->num_bands; ++j)
    {
        Image *down = (Image *)malloc(sizeof(Image));
        *down = downsample(current_img);

        Image *up = (Image *)malloc(sizeof(Image));
        *up = upsample(down);

        img_laplacians[j] = compute_laplacian(current_img, up);
        free(up);

        if (j > 0)
        {
            free(current_img);
        }

        current_img = down;
    }

    img_laplacians[b->num_bands] = *current_img;

    Image *mask_gaussian = (Image *)malloc((b->num_bands + 1) * sizeof(Image));
    Image *mask = mask_img;

    for (int j = 0; j < b->num_bands; ++j)
    {
        mask_gaussian[j] = *mask;

        Image *sampled = (Image *)malloc(sizeof(Image));
        *sampled = downsample(mask);

        if (j > 0)
        {
            free(mask);
        }

        mask = sampled;
    }

    mask_gaussian[b->num_bands] = *mask;

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
            thread_data[i].level_width = img_laplacians[level].width;
            thread_data[i].level_height = img_laplacians[level].height;
            thread_data[i].level = level;
            thread_data[i].img_laplacians = img_laplacians;
            thread_data[i].mask_gaussian = mask_gaussian;
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

    free(img_laplacians);
    free(mask_gaussian);
}

void *normalize_worker(void *args)
{
    NormalizeWorkerArgs *wArgs = (NormalizeWorkerArgs *)args;
    for (int y = wArgs->start_row; y < wArgs->end_row; ++y)
    {
        for (int x = 0; x < wArgs->output_width; ++x)
        {
            int maskIndex = x + (y * wArgs->output_width);
            if (maskIndex < image_size(wArgs->out_mask[wArgs->level]))
            {
                int w = wArgs->out_mask[wArgs->level]->data[maskIndex] + 1;

                for (int z = 0; z < CHANNELS; z++)
                {
                    int imgIndex = (x + (y * wArgs->output_width)) * CHANNELS + z;
                    if (imgIndex < image_size(wArgs->out[wArgs->level]))
                    {
                        wArgs->out[wArgs->level]->data[imgIndex] =
                            (unsigned char)(((int)wArgs->out[wArgs->level]->data[imgIndex] * 256) / w);
                    }
                }
            }
        }
    }
    return NULL;
}

void *blend_worker(void *args)
{
    BlendWorkerArgs *wArgs = (BlendWorkerArgs *)args;
    for (int i = wArgs->start_index; i < wArgs->end_index; ++i)
    {
        if (i < wArgs->out_size)
        {
            wArgs->blended_image->data[i] = clamp(
                wArgs->blended_image->data[i] + wArgs->out_level->data[i], 0, 255);
        }
    }
    return NULL;
}

void blend(Blender *b)
{
    clock_t start = clock();

    int num_threads = get_cpus_count();

    for (int level = 0; level < b->num_bands; ++level)
    {
        pthread_t threads[num_threads];
        NormalizeWorkerArgs threadArgs[num_threads];

        int rowsPerThread = b->output_size.height / num_threads;
        int remainingRows = b->output_size.height % num_threads;

        for (int i = 0; i < num_threads; ++i)
        {
            threadArgs[i].start_row = i * rowsPerThread;
            threadArgs[i].end_row = threadArgs[i].start_row + rowsPerThread + (i == num_threads - 1 ? remainingRows : 0);
            threadArgs[i].output_width = b->output_size.width;
            threadArgs[i].level = level;
            threadArgs[i].out = b->out;
            threadArgs[i].out_mask = b->out_mask;

            pthread_create(&threads[i], NULL, normalize_worker, &threadArgs[i]);
        }

        for (int i = 0; i < num_threads; ++i)
        {
            pthread_join(threads[i], NULL);
        }
    }

    Image *blended_image = b->out[b->num_bands];
    Image ubi;

    for (int level = b->num_bands; level > 0; --level)
    {
        ubi = upsample(blended_image);
        blended_image = &ubi;

        int out_size = image_size(b->out[level - 1]);
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

    b->result = (Image *)malloc(sizeof(Image));
    b->result->data = (unsigned char *)malloc(image_size(blended_image) * sizeof(unsigned char));
    b->result->channels = blended_image->channels;
    b->result->width = blended_image->width;
    b->result->height = blended_image->height;
    memmove(b->result->data, blended_image->data, image_size(blended_image) * sizeof(unsigned char));

    free(ubi.data);

    if (DEBUG)
    {
        clock_t end = clock();
        double duration = (double)(end - start) / CLOCKS_PER_SEC;
        printf("Elapsed time for blend: %.2f seconds\n", duration);
    }
}
