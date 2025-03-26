#include "jpeg.h"

typedef enum
{
    DOWNSAMPLE,
    UPSAMPLE,
    LAPLACIAN,
    FEED,
    BLEND
} OperatorType;

typedef struct
{
    int x;
    int y;
} Point;

typedef struct
{
    int x;
    int y;
    int width;
    int height;
} Rect;

Point br(Rect r);
Rect create_rect(int x, int y, int width, int height);

typedef struct
{
    int num_bands;
    Rect output_size;
    int *out_width_levels;
    int *out_height_levels;
    Image *out;
    Image *out_mask;
    Image result;
    Image *img_laplacians;
    Image *mask_gaussian;
} Blender;

Blender *create_blender(Rect out_size, int nb);
int feed(Blender *b, Image *img, Image *maskImg, Point tl);
void blend(Blender *b);
void destroy_blender(Blender *blender);



typedef struct
{
    int new_width;
    int new_height;
    const Image *img;
    unsigned char *sampled;
} SamplingThreadData;

typedef struct
{
    unsigned char *original_data;
    unsigned char *upsampled_data;
    unsigned char *laplacian_data;
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
    Image *img_laplacians;
    Image *mask_gaussian;
    Image *out;
    Image *out_mask;
} FeedThreadData;

typedef struct
{
    int out_size;
    Image blended_image;
    Image out_level;
} BlendThreadData;

typedef union 
{
    SamplingThreadData *std;
    LaplacianThreadData *ltd;
    FeedThreadData *ftd;
    BlendThreadData *btd;
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
Image create_image_mask(int width, int height, float range, int left, int right);
int save_image(const Image *img, char *out_filename);
int image_size(Image *img);
void destroy_image(Image *img);
Image upsample(const Image *img);
Image downsample(const Image *img);
void crop_image(Image *img, int cut_top, int cut_bottom, int cut_left, int cut_right);
void parallel_operator(OperatorType operatorType, ParallelOperatorArgs *arg);