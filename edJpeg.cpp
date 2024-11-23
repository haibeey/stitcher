#include <turbojpeg.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include "edJpeg.h"
#include "utils.h"

bool decompressJPEG(const char *filename, std::vector<unsigned char> &imgBuf, int &width, int &height)
{
    tjhandle handle = tjInitDecompress();
    if (!handle)
        return false;

    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile)
    {
        tjDestroy(handle);
        return false;
    }

    std::vector<unsigned char> jpegBuf((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
    inFile.close();

    int jpegSubsamp;
    if (tjDecompressHeader2(handle, jpegBuf.data(), jpegBuf.size(), &width, &height, &jpegSubsamp) < 0)
    {
        tjDestroy(handle);
        return false;
    }

    imgBuf.resize(width * height * 3);

    if (tjDecompress2(handle, jpegBuf.data(), jpegBuf.size(), imgBuf.data(), width, 0, height, TJPF_RGB, TJFLAG_FASTDCT) < 0)
    {
        tjDestroy(handle);
        return false;
    }
    else
    {
        tjDestroy(handle);
        return true;
    }
}

std::vector<unsigned char> convertRGBToGray(const std::vector<unsigned char> &rgbBuffer, int width, int height)
{
    std::vector<unsigned char> grayBuffer(width * height);

    for (int i = 0; i < width * height; ++i)
    {
        unsigned char r = rgbBuffer[i * 3];
        unsigned char g = rgbBuffer[i * 3 + 1];
        unsigned char b = rgbBuffer[i * 3 + 2];

        grayBuffer[i] = static_cast<unsigned char>(0.299 * r + 0.587 * g + 0.114 * b);
    }

    return grayBuffer;
}

bool compressJPEG(const char *outputFilename, unsigned char *imgBuf, int width, int height, int quality)
{
    tjhandle handle = tjInitCompress();
    if (!handle)
        return false;

    unsigned char *jpegBuf = nullptr;
    unsigned long jpegSize = 0;

    if (tjCompress2(handle, imgBuf, width, 0, height, TJPF_RGB, &jpegBuf, &jpegSize, TJSAMP_444, quality, TJFLAG_FASTDCT) < 0)
    {
        tjDestroy(handle);
        return false;
    }

    std::ofstream outFile(outputFilename, std::ios::binary);
    if (!outFile)
    {
        tjFree(jpegBuf);
        tjDestroy(handle);
        return false;
    }

    outFile.write(reinterpret_cast<const char *>(jpegBuf), jpegSize);
    outFile.close();

    tjFree(jpegBuf);
    tjDestroy(handle);

    return true;
}

bool compressGrayscaleJPEG(const char *outputFilename, unsigned char *imgBuf, int width, int height, int quality)
{
    tjhandle handle = tjInitCompress();
    if (!handle)
        return false;

    unsigned char *jpegBuffer = nullptr;
    unsigned long jpegSize = 0;

    if (tjCompress2(handle, imgBuf, width, 0, height, TJPF_GRAY, &jpegBuffer, &jpegSize, TJSAMP_GRAY, quality, TJFLAG_FASTDCT) < 0)
    {
        std::cerr << "Compression failed: " << tjGetErrorStr() << std::endl;
        tjDestroy(handle);
        return false;
    }

    std::ofstream outFile(outputFilename, std::ios::binary);
    if (!outFile)
    {
        tjDestroy(handle);
        tjFree(jpegBuffer);
        return false;
    }
    outFile.write(reinterpret_cast<char *>(jpegBuffer), jpegSize);
    outFile.close();

    tjDestroy(handle);
    tjFree(jpegBuffer);
    return true;
}

std::vector<unsigned char> createMask(int width, int height, float range, bool left, bool right)
{
    std::vector<unsigned char> grayBuffer(width * height, 255);
    if (!(left || right))
        return grayBuffer;

    int cut = static_cast<int>(range * width);

    if (left)
    {
        for (int i = 0; i < cut; ++i)
        {
            for (int j = 0; j < height; j++)
                grayBuffer[i + width * j] = 0;
        }
    }

    if (right)
    {
        for (int i = 0; i < cut; ++i)
        {
            for (int j = 0; j < height; j++)
                grayBuffer[(width - i - 1) + width * j] = 0;
        }
    }

    return grayBuffer;
}

std::vector<unsigned char> createVerticalMask(int width, int height, float range, bool top, bool bottom)
{
    std::vector<unsigned char> grayBuffer(width * height, 255);
    if (!(top || bottom))
        return grayBuffer;

    int cut = static_cast<int>(range * height * width);

    if (top)
    {
        std::fill(grayBuffer.begin(), grayBuffer.begin() + cut, 0);
    }

    if (bottom)
    {
        std::fill(grayBuffer.end() - cut, grayBuffer.end(), 0);
    }

    return grayBuffer;
}

void addBorderToImage(std::vector<unsigned char> &img, int &width, int &height,
                      int borderTop, int borderBottom, int borderLeft, int borderRight,
                      int channels, BorderType borderType)
{
    // Define a constant border color (default to black/zero for all channels)
    std::vector<unsigned char> borderColor(channels, 0);

    // Calculate new dimensions
    int newWidth = width + borderLeft + borderRight;
    int newHeight = height + borderTop + borderBottom;

    // Create a new image buffer
    std::vector<unsigned char> borderedImage(newWidth * newHeight * channels, 0);

    // Fill the bordered image based on the border type
    for (int y = 0; y < newHeight; ++y)
    {
        for (int x = 0; x < newWidth; ++x)
        {
            int newIdx = (y * newWidth + x) * channels;

            if (x >= borderLeft && x < (width + borderLeft) && y >= borderTop && y < (height + borderTop))
            {
                // Copy the original image pixels
                int origX = x - borderLeft;
                int origY = y - borderTop;
                int origIdx = (origY * width + origX) * channels;

                for (int c = 0; c < channels; ++c)
                {
                    borderedImage[newIdx + c] = img[origIdx + c];
                }
            }
            else if (borderType == BORDER_CONSTANT)
            {
                // Set constant border color
                for (int c = 0; c < channels; ++c)
                {
                    borderedImage[newIdx + c] = borderColor[c];
                }
            }
            else if (borderType == BORDER_REFLECT)
            {
                // Reflect border pixels
                int origX = std::clamp(x - borderLeft, 0, width - 1);
                if (x < borderLeft)
                    origX = borderLeft - x - 1;
                if (x >= (width + borderLeft))
                    origX = width - (x - (width + borderLeft)) - 1;

                int origY = std::clamp(y - borderTop, 0, height - 1);
                if (y < borderTop)
                    origY = borderTop - y - 1;
                if (y >= (height + borderTop))
                    origY = height - (y - (height + borderTop)) - 1;

                int origIdx = (origY * width + origX) * channels;

                for (int c = 0; c < channels; ++c)
                {
                    borderedImage[newIdx + c] = img[origIdx + c];
                }
            }
        }
    }

    img = std::move(borderedImage);
    width = newWidth;
    height = newHeight;
}

// //g++-14 -o libjpegtest -I/usr/local/include -L/usr/local/lib -lturbojpeg  libjpegtest.cpp
//  g++-14 -o stitch -I/usr/local/include -L/usr/local/lib -lturbojpeg  stitch.cpp laplaceBlending.cpp edJpeg.cpp  && ./stitch