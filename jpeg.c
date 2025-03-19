#include "turbojpeg.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "utils.h"
#include "jpeg.h"


Image decompress_jpeg(const char *filename)
{
    Image result;
    result.channels = CHANNELS;
    tjhandle handle = tjInitDecompress();
    if (!handle)
    {
        fprintf(stderr, "Failed to initialize TurboJPEG decompressor.\n");
        return result;
    }

    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        tjDestroy(handle);
        return result;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char *jpegBuf = (unsigned char *)malloc(fileSize);
    if (!jpegBuf)
    {
        fprintf(stderr, "Failed to allocate memory for JPEG buffer.\n");
        fclose(file);
        tjDestroy(handle);
        return result;
    }

    fread(jpegBuf, 1, fileSize, file);
    fclose(file);

    int jpegSubsamp;
    if (tjDecompressHeader2(handle, jpegBuf, fileSize, &result.width, &result.height, &jpegSubsamp) < 0)
    {
        fprintf(stderr, "Failed to read JPEG header: %s\n", tjGetErrorStr());
        free(jpegBuf);
        tjDestroy(handle);
        return result;
    }

    result.data = (unsigned char *)malloc(result.width * result.height * 3);
    if (!result.data )
    {
        fprintf(stderr, "Failed to allocate memory for image buffer.\n");
        free(jpegBuf);
        tjDestroy(handle);
        return result;
    }


    if (tjDecompress2(handle, jpegBuf, fileSize, result.data , result.width, 0, result.height, TJPF_RGB, TJFLAG_FASTDCT) < 0)
    {
        fprintf(stderr, "Failed to decompress JPEG: %s\n", tjGetErrorStr());
        free(jpegBuf);
        free(result.data);
        tjDestroy(handle);
        return result;
    }

    free(jpegBuf);
    tjDestroy(handle);
    return result;
}

Image convert_RGB_to_gray(const Image *img)
{
    Image result;
    int numPixels = img->width * img->height;
    unsigned char *grayBuffer = (unsigned char *)malloc(numPixels);
    if (!grayBuffer)
    {
        fprintf(stderr, "Failed to allocate memory for grayscale buffer.\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < numPixels; ++i)
    {
        unsigned char r = img->data[i * 3];
        unsigned char g = img->data[i * 3 + 1];
        unsigned char b = img->data[i * 3 + 2];
        grayBuffer[i] = (unsigned char)(0.299 * r + 0.587 * g + 0.114 * b);
    }

    result.channels = 1;
    result.width = img->width;
    result.height = img->height;
    result.data = grayBuffer;


    return result;
}

int compress_jpeg(const char *outputFilename, const Image *img, int quality)
{
    tjhandle handle = tjInitCompress();
    if (!handle)
    {
        fprintf(stderr, "Failed to initialize TurboJPEG compressor.\n");
        return 0;
    }

    unsigned char *jpegBuf = NULL;
    unsigned long jpegSize = 0;

    if (tjCompress2(handle, img->data, img->width, 0, img->height, TJPF_RGB, &jpegBuf, &jpegSize, TJSAMP_444, quality, TJFLAG_FASTDCT) < 0)
    {
        fprintf(stderr, "Failed to compress JPEG: %s\n", tjGetErrorStr());
        tjDestroy(handle);
        return 0;
    }

    FILE *outFile = fopen(outputFilename, "wb");
    if (!outFile)
    {
        fprintf(stderr, "Failed to open output file: %s\n", outputFilename);
        tjFree(jpegBuf);
        tjDestroy(handle);
        return 0;
    }

    fwrite(jpegBuf, 1, jpegSize, outFile);
    fclose(outFile);

    tjFree(jpegBuf);
    tjDestroy(handle);

    return 1;
}

int compress_grayscale_jpeg(const char *outputFilename, const Image *img, int quality)
{
    tjhandle handle = tjInitCompress();
    if (!handle)
    {
        fprintf(stderr, "Failed to initialize TurboJPEG compressor.\n");
        return 0;
    }

    unsigned char *jpegBuf = NULL;
    unsigned long jpegSize = 0;

    if (tjCompress2(handle, img->data, img->width, 0, img->height, TJPF_GRAY, &jpegBuf, &jpegSize, TJSAMP_GRAY, quality, TJFLAG_FASTDCT) < 0)
    {
        fprintf(stderr, "Failed to compress grayscale JPEG: %s\n", tjGetErrorStr());
        tjDestroy(handle);
        return 0;
    }

    FILE *outFile = fopen(outputFilename, "wb");
    if (!outFile)
    {
        fprintf(stderr, "Failed to open output file: %s\n", outputFilename);
        tjFree(jpegBuf);
        tjDestroy(handle);
        return 0;
    }

    fwrite(jpegBuf, 1, jpegSize, outFile);
    fclose(outFile);

    tjFree(jpegBuf);
    tjDestroy(handle);

    return 1;
}

Image create_mask(int width, int height, float range, int left, int right)
{
    Image result;
    unsigned char *grayBuffer = (unsigned char *)malloc(width * height);
    if (!grayBuffer)
    {
        fprintf(stderr, "Failed to allocate memory for mask buffer.\n");
        exit(EXIT_FAILURE);
    }

    memset(grayBuffer, 255, width * height);

    if (!(left || right))
        return result;

    int cut = (int)(range * width);

    if (left)
    {
        for (int i = 0; i < cut; ++i)
        {
            for (int j = 0; j < height; ++j)
            {
                grayBuffer[i + width * j] = 0;
            }
        }
    }

    if (right)
    {
        for (int i = 0; i < cut; ++i)
        {
            for (int j = 0; j < height; ++j)
            {
                grayBuffer[(width - i - 1) + width * j] = 0;
            }
        }
    }

    result.data = grayBuffer;
    result.height = height;
    result.width = width;
    result.channels = 1;

    return result;
}

Image create_vertical_mask(int width, int height, float range, int top, int bottom)
{
    Image result;
    unsigned char *grayBuffer = (unsigned char *)malloc(width * height);
    if (!grayBuffer)
    {
        return result;
    }
    memset(grayBuffer, 255, width * height);

    if (!(top || bottom))
    {
        return result;
    }

    int cut = (int)(range * height * width);

    if (top)
    {
        memset(grayBuffer, 0, cut);
    }

    if (bottom)
    {
        memset(grayBuffer + (width * height - cut), 0, cut);
    }

    result.data = grayBuffer;
    result.height = height;
    result.width = width;
    result.channels = 1;

    return result;
}

void add_border_to_image(Image *img,
    int borderTop, int borderBottom, int borderLeft, int borderRight,
    int channels, BorderType borderType)
{
    int newWidth = img->width + borderLeft + borderRight;
    int newHeight = img->height + borderTop + borderBottom;

    unsigned char *borderedImage = (unsigned char *)calloc(newWidth * newHeight * channels, sizeof(unsigned char));
    if (!borderedImage)
    {
        return;
    }

    for (int y = 0; y < newHeight; ++y)
    {
        for (int x = 0; x < newWidth; ++x)
        {
            int newIdx = (y * newWidth + x) * channels;

            if (x >= borderLeft && x < (img->width + borderLeft) && y >= borderTop && y < (img->height + borderTop))
            {
                int origX = x - borderLeft;
                int origY = y - borderTop;
                int origIdx = (origY * (img->width) + origX) * channels;

                for (int c = 0; c < channels; ++c)
                {
                    borderedImage[newIdx + c] = img->data[origIdx + c];
                }
            }
            else if (borderType == BORDER_CONSTANT)
            {
                int origX = clamp(x - borderLeft, 0, img->width - 1);
                int origY = clamp(y - borderTop, 0, img->height - 1);

                int origIdx = (origY * (img->width) + origX) * channels;

                for (int c = 0; c < channels; ++c)
                {
                    borderedImage[newIdx + c] = img->data[origIdx + c];
                }
            }
            else if (borderType == BORDER_REFLECT)
            {
                int origX = clamp(x - borderLeft, 0, img->width - 1);
                if (x < borderLeft)
                    origX = borderLeft - x - 1;
                if (x >= (img->width + borderLeft))
                    origX = img->width - (x - (img->width + borderLeft)) - 1;

                int origY = clamp(y - borderTop, 0, img->height - 1);
                if (y < borderTop)
                    origY = borderTop - y - 1;
                if (y >= (img->height + borderTop))
                    origY = img->height - (y - (img->height + borderTop)) - 1;

                int origIdx = (origY * (img->width) + origX) * channels;

                for (int c = 0; c < channels; ++c)
                {
                    borderedImage[newIdx + c] = img->data[origIdx + c];
                }
            }
        }
    }


    free(img->data);
    img->data = borderedImage;
    img->width = newWidth;
    img->height = newHeight;
}

void crop_image_buf(Image *img,int cut_top, int cut_bottom, int cut_left, int cut_right,int channels)
{
    int new_width = img->width - cut_left - cut_right;
    int new_height = img->height - cut_top - cut_bottom;

    if (new_width <= 0 || new_height <= 0) {
        return;
    }

    unsigned char *cropped = (unsigned char *)malloc(new_width * new_height * channels);

    if (!cropped) {
        return; 
    }

    for (int y = 0; y < new_height; y++) {
        int src_y = y + cut_top;
        int src_offset = (src_y * (img->width ) + cut_left) * channels;
        int dest_offset = y * new_width * channels;
        memcpy(cropped + dest_offset, img->data + src_offset, new_width * channels);
    }

    free(img->data);
    img->data = cropped;
    img->width = new_width;
    img->height = new_height;
}
