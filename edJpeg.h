
typedef enum {
    BORDER_CONSTANT,
    BORDER_REFLECT
} BorderType;

int decompress_jpeg(const char *filename, unsigned char **imgBuf, int *width, int *height);
unsigned char *convert_RGB_to_gray(const unsigned char *rgbBuffer, int width, int height) ;
int compress_jpeg(const char *outputFilename, unsigned char *imgBuf, int width, int height, int quality);
int compress_grayscale_jpeg(const char *outputFilename, unsigned char *imgBuf, int width, int height, int quality);
unsigned char *create_mask(int width, int height, float range, int left, int right);
unsigned char* create_vertical_mask(int width, int height, float range, int top, int bottom);
void add_border_to_image(unsigned char** img, int* width, int* height,
                      int borderTop, int borderBottom, int borderLeft, int borderRight,
                      int channels, BorderType borderType);

void crop_image(unsigned char **img, int *width, int *height,
                         int cut_top, int cut_bottom, int cut_left, int cut_right,
                         int channels);