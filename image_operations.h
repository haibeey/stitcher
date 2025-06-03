#ifdef __cplusplus
extern "C" {
#endif


#include "turbojpeg.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include "jpeg.h"
#include "utils.h"

static const float GAUSSIAN_KERNEL[5][5] = {
    {1.0 / 256, 4.0 / 256, 6.0 / 256, 4.0 / 256, 1.0 / 256},
    {4.0 / 256, 16.0 / 256, 24.0 / 256, 16.0 / 256, 4.0 / 256},
    {6.0 / 256, 24.0 / 256, 36.0 / 256, 24.0 / 256, 6.0 / 256},
    {4.0 / 256, 16.0 / 256, 24.0 / 256, 16.0 / 256, 4.0 / 256},
    {1.0 / 256, 4.0 / 256, 6.0 / 256, 4.0 / 256, 1.0 / 256}};

static const float WEIGHT_EPS = 1e-5f;



#define MAX_BANDS 7
typedef enum
{
    DOWNSAMPLE,
    UPSAMPLE,
    LAPLACIAN,
    FEED,
    BLEND,
    NORMALIZE
} OperatorType;

typedef struct
{
    int x;
    int y;
} StitchPoint;

typedef struct
{
    int x;
    int y;
    int width;
    int height;
} StitchRect;

StitchPoint br(StitchRect r);

typedef struct
{
    float upsample_factor;
    int new_width;
    int new_height;
    void *img;
    void *sampled;
    ImageType image_type;
} SamplingThreadData;

typedef struct
{
    ImageS *original;
    ImageS *upsampled;
    int total_size;
} LaplacianThreadData;

typedef struct
{
    int rows;
    int cols;
    int x_tl;
    int y_tl;
    int out_level_width;
    int out_level_height;
    int level_width;
    int level_height;
    int level;
    ImageS *img_laplacians;
    ImageS *mask_gaussian;
    ImageF *out;
    ImageF *out_mask;
} FeedThreadData;

typedef struct
{
    int output_width;
    int level;
    ImageF *out;
    ImageF *out_mask;
    ImageS *final_out;
} NormalThreadData;

typedef struct
{
    int out_size;
    ImageS blended_image;
    ImageS out_level;
} BlendThreadData;

typedef union
{
    SamplingThreadData *std;
    LaplacianThreadData *ltd;
    FeedThreadData *ftd;
    BlendThreadData *btd;
    NormalThreadData *ntd;
} WorkerThreadArgs;

typedef struct
{
    int rows;
    WorkerThreadArgs *workerThreadArgs;
} ParallelOperatorArgs;

typedef struct
{
    int start_index;
    int end_index;
    WorkerThreadArgs *workerThreadArgs;
} ThreadArgs;


Image create_image(const char *filename);

void distance_transform(Image *mask);

Image create_empty_image(int width, int height, int channels);
ImageS create_empty_image_s(int width, int height, int channels);
ImageF create_empty_image_f(int width, int height, int channels);

Image create_image_mask(int width, int height, float range, int left, int right);
int save_image(const Image *img, const char *out_filename);

int image_size(Image *img);
int image_size_s(ImageS *img);
int image_size_f(ImageF *img);

void destroy_image(Image *img);
void destroy_image_s(ImageS *img);
void destroy_image_f(ImageF *img);

Image upsample( Image *img,float upsample_factor);
ImageS upsample_image_s( ImageS *img,float upsample_factor);
ImageF upsample_image_f( ImageF *img,float upsample_factor);

void *down_sample_operation(void *args);
void *down_sample_operation_s(void *args);
void *down_sample_operation_f(void *args);

void *upsample_worker(void *args);
void *upsample_worker_s(void *args);
void *upsample_worker_f(void *args);

Image downsample(Image *img);
ImageS downsample_s(ImageS *img);
ImageF downsample_f(ImageF *img);

void crop_image(Image *img, int cut_top, int cut_bottom, int cut_left, int cut_right);
void parallel_operator(OperatorType operatorType, ParallelOperatorArgs *arg);


#ifdef __cplusplus
}
#endif
