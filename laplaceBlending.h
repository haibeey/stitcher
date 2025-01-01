
typedef struct
{
    unsigned char *data;
    int width;
    int height;
    int channels;
} Image;

Image *create_image(const char *filename);
Image *create_image_mask(int width, int height, float range, int left, int right);
int save_image(Image *img, char *out_filename);
int image_size(Image *img);
void destroy_image(Image *img);
Image upsample(Image *img);
Image downsample(Image *img);
Image compute_laplacian(Image *original, Image *upsampled);
void crop_image(Image *img, int cut_top, int cut_bottom, int cut_left, int cut_right);

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
    Image **out;
    Image **out_mask;
    Image *result;
} Blender;

Blender *create_blender(Rect out_size, int nb);
void feed(Blender *b, Image *img, Image *maskImg, Point tl);
void blend(Blender *b);
void destroy_blender(Blender *blender);

typedef struct
{
    int start_row;
    int end_row;
    int new_width;
    int new_height;
    Image *img;
    unsigned char *sampled;
} SamplingThreadData;

typedef struct
{
    unsigned char *original_data;
    unsigned char *upsampled_data;
    unsigned char *laplacian_data;
    int total_size;
    int start_index;
    int end_index;
} LaplacianWorkerArgs;

typedef struct
{
    int start_row;
    int end_row;
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
    Image **out;
    Image **out_mask;
} FeedWorkerArgs;

typedef struct
{
    int start_row;
    int end_row;
    int output_width;
    int level;
    Image **out;
    Image **out_mask;
} NormalizeWorkerArgs;

typedef struct
{
    int start_index;
    int end_index;
    int out_size;
    Image *blended_image;
    Image *out_level;
} BlendWorkerArgs;