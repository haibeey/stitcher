#ifndef CHANNELS
#define CHANNELS 3
#endif

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