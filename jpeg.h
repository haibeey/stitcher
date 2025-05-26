#ifndef IMAGE_HEADERS
#define IMAGE_HEADERS
#define RGB_CHANNELS 3
#define GRAY_CHANNELS 1

typedef enum {
    BORDER_CONSTANT,
    BORDER_REFLECT
} BorderType;


typedef struct
{
    unsigned char *data;
    int width;
    int height;
    int channels;
} Image;

typedef struct
{
    float *data;
    int width;
    int height;
    int channels;
} ImageF;

typedef struct
{
    short *data;
    int width;
    int height;
    int channels;
} ImageS;


typedef enum {
    IMAGE,
    IMAGES,
    IMAGEF
} ImageType;

Image decompress_jpeg(const char *filename);
Image convert_RGB_to_gray(const Image *img);
int compress_jpeg(const char *outputFilename, const Image *img, int quality);
int compress_grayscale_jpeg(const char *outputFilename, const Image *img, int quality);
Image create_mask(int width, int height, float range, int left, int right);
Image create_vertical_mask(int width, int height, float range, int top, int bottom);
void add_border_to_image(Image *img,
                      int borderTop, int borderBottom, int borderLeft, int borderRight,
                      int channels, BorderType borderType);

void crop_image_buf(Image *img,int cut_top, int cut_bottom, int cut_left, int cut_right,int channels);
void convert_image_to_image_f(Image* in , ImageF *out);
void convert_image_to_image_s(Image* in , ImageS *out);
void convert_imagef_to_image(ImageF* in , Image *out);
void convert_images_to_image(ImageS* in , Image *out);
#endif
