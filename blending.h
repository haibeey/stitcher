#ifdef __cplusplus
extern "C" {
#endif


#include "image_operations.h"

typedef enum{
    MULTIBAND,
    FEATHER
} BlenderType;

typedef struct
{
    int num_bands;
    Rect output_size;
    Rect real_out_size;
    int *out_width_levels;
    int *out_height_levels;
    ImageF *out;
    ImageF *out_mask;
    ImageS *final_out;
    Image result;
    ImageS *img_laplacians;
    ImageS *mask_gaussian;
    BlenderType blender_type;
    float sharpness;
    int do_distance_transform;
} Blender;

Blender *create_blender(BlenderType blender_type, Rect out_size, int nb);
int feed(Blender *b, Image *img, Image *maskImg, Point tl);
void blend(Blender *b);
void destroy_blender(Blender *blender);


#ifdef __cplusplus
}
#endif
