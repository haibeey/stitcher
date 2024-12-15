#include "turbojpeg.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "utils.h"
#include "edJpeg.h"

int decompress_jpeg(const char *filename, unsigned char **imgBuf, int *width, int *height)
{
    tjhandle handle = tjInitDecompress();
    if (!handle)
    {
        fprintf(stderr, "Failed to initialize TurboJPEG decompressor.\n");
        return 0;
    }

    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        tjDestroy(handle);
        return 0;
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
        return 0;
    }

    fread(jpegBuf, 1, fileSize, file);
    fclose(file);

    int jpegSubsamp;
    if (tjDecompressHeader2(handle, jpegBuf, fileSize, width, height, &jpegSubsamp) < 0)
    {
        fprintf(stderr, "Failed to read JPEG header: %s\n", tjGetErrorStr());
        free(jpegBuf);
        tjDestroy(handle);
        return 0;
    }

    *imgBuf = (unsigned char *)malloc((*width) * (*height) * 3);
    if (!*imgBuf)
    {
        fprintf(stderr, "Failed to allocate memory for image buffer.\n");
        free(jpegBuf);
        tjDestroy(handle);
        return 0;
    }

    if (tjDecompress2(handle, jpegBuf, fileSize, *imgBuf, *width, 0, *height, TJPF_RGB, TJFLAG_FASTDCT) < 0)
    {
        fprintf(stderr, "Failed to decompress JPEG: %s\n", tjGetErrorStr());
        free(jpegBuf);
        free(*imgBuf);
        tjDestroy(handle);
        return 0;
    }

    free(jpegBuf);
    tjDestroy(handle);
    return 1;
}

unsigned char *convert_RGB_to_gray(const unsigned char *rgbBuffer, int width, int height)
{
    int numPixels = width * height;
    unsigned char *grayBuffer = (unsigned char *)malloc(numPixels);
    if (!grayBuffer)
    {
        fprintf(stderr, "Failed to allocate memory for grayscale buffer.\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < numPixels; ++i)
    {
        unsigned char r = rgbBuffer[i * 3];
        unsigned char g = rgbBuffer[i * 3 + 1];
        unsigned char b = rgbBuffer[i * 3 + 2];
        grayBuffer[i] = (unsigned char)(0.299 * r + 0.587 * g + 0.114 * b);
    }

    return grayBuffer;
}

int compress_jpeg(const char *outputFilename, unsigned char *imgBuf, int width, int height, int quality)
{
    tjhandle handle = tjInitCompress();
    if (!handle)
    {
        fprintf(stderr, "Failed to initialize TurboJPEG compressor.\n");
        return 0;
    }

    unsigned char *jpegBuf = NULL;
    unsigned long jpegSize = 0;

    if (tjCompress2(handle, imgBuf, width, 0, height, TJPF_RGB, &jpegBuf, &jpegSize, TJSAMP_444, quality, TJFLAG_FASTDCT) < 0)
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

int compress_grayscale_jpeg(const char *outputFilename, unsigned char *imgBuf, int width, int height, int quality)
{
    tjhandle handle = tjInitCompress();
    if (!handle)
    {
        fprintf(stderr, "Failed to initialize TurboJPEG compressor.\n");
        return 0;
    }

    unsigned char *jpegBuf = NULL;
    unsigned long jpegSize = 0;

    if (tjCompress2(handle, imgBuf, width, 0, height, TJPF_GRAY, &jpegBuf, &jpegSize, TJSAMP_GRAY, quality, TJFLAG_FASTDCT) < 0)
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

unsigned char *create_mask(int width, int height, float range, int left, int right)
{
    unsigned char *grayBuffer = (unsigned char *)malloc(width * height);
    if (!grayBuffer)
    {
        fprintf(stderr, "Failed to allocate memory for mask buffer.\n");
        exit(EXIT_FAILURE);
    }

    memset(grayBuffer, 255, width * height);

    if (!(left || right))
        return grayBuffer;

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

    return grayBuffer;
}

unsigned char *create_vertical_mask(int width, int height, float range, int top, int bottom)
{
    unsigned char *grayBuffer = (unsigned char *)malloc(width * height);
    if (!grayBuffer)
    {
        return NULL;
    }
    memset(grayBuffer, 255, width * height);

    if (!(top || bottom))
    {
        return grayBuffer;
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

    return grayBuffer;
}

void add_border_to_image(unsigned char **img, int *width, int *height,
                         int borderTop, int borderBottom, int borderLeft, int borderRight,
                         int channels, BorderType borderType)
{
    unsigned char *borderColor = (unsigned char *)calloc(channels, sizeof(unsigned char));

    int newWidth = *width + borderLeft + borderRight;
    int newHeight = *height + borderTop + borderBottom;

    unsigned char *borderedImage = (unsigned char *)calloc(newWidth * newHeight * channels, sizeof(unsigned char));
    if (!borderedImage)
    {
        free(borderColor);
        return;
    }

    for (int y = 0; y < newHeight; ++y)
    {
        for (int x = 0; x < newWidth; ++x)
        {
            int newIdx = (y * newWidth + x) * channels;

            if (x >= borderLeft && x < (*width + borderLeft) && y >= borderTop && y < (*height + borderTop))
            {
                int origX = x - borderLeft;
                int origY = y - borderTop;
                int origIdx = (origY * (*width) + origX) * channels;

                for (int c = 0; c < channels; ++c)
                {
                    borderedImage[newIdx + c] = (*img)[origIdx + c];
                }
            }
            else if (borderType == BORDER_CONSTANT)
            {
                for (int c = 0; c < channels; ++c)
                {
                    borderedImage[newIdx + c] = 0;
                }
            }
            else if (borderType == BORDER_REFLECT)
            {
                int origX = clamp(x - borderLeft, 0, *width - 1);
                if (x < borderLeft)
                    origX = borderLeft - x - 1;
                if (x >= (*width + borderLeft))
                    origX = *width - (x - (*width + borderLeft)) - 1;

                int origY = clamp(y - borderTop, 0, *height - 1);
                if (y < borderTop)
                    origY = borderTop - y - 1;
                if (y >= (*height + borderTop))
                    origY = *height - (y - (*height + borderTop)) - 1;

                int origIdx = (origY * (*width) + origX) * channels;

                for (int c = 0; c < channels; ++c)
                {
                    borderedImage[newIdx + c] = (*img)[origIdx + c];
                }
            }
        }
    }

    free(*img);
    *img = borderedImage;
    *width = newWidth;
    *height = newHeight;

    free(borderColor);
}

void crop_image(unsigned char **img, int *width, int *height,
                int cut_top, int cut_bottom, int cut_left, int cut_right,
                int channels)
{

}