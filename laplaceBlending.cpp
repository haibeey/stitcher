#include <turbojpeg.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include "utils.h"

const int CHANNELS = 3;
const int GAUSSIAN_SIZE = 5;
const float GAUSSIAN_SIGMA = 1.0f;

std::vector<unsigned char> applyGaussianFilter(const std::vector<unsigned char> &img, int width, int height)
{
    std::vector<unsigned char> blurred(width * height);
    int kernelSize = 3;
    float kernel[3][3] = {
        {1 / 16.0f, 2 / 16.0f, 1 / 16.0f},
        {2 / 16.0f, 4 / 16.0f, 2 / 16.0f},
        {1 / 16.0f, 2 / 16.0f, 1 / 16.0f}};

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            float sum = 0.0f;
            for (int ky = -1; ky <= 1; ++ky)
            {
                for (int kx = -1; kx <= 1; ++kx)
                {
                    int nx = clamp(x + kx, 0, width - 1);
                    int ny = clamp(y + ky, 0, height - 1);
                    sum += img[ny * width + nx] * kernel[ky + 1][kx + 1];
                }
            }
            blurred[y * width + x] = static_cast<unsigned char>(sum);
        }
    }
    return blurred;
}

std::vector<unsigned char> downsampleWithBilinear(const std::vector<unsigned char> &img, int width, int height)
{
    int newWidth = width / 2;
    int newHeight = height / 2;
    std::vector<unsigned char> downsampled(newWidth * newHeight);

    for (int y = 0; y < newHeight; ++y)
    {
        for (int x = 0; x < newWidth; ++x)
        {
            float sum = 0;
            sum += img[(2 * y * width + 2 * x)];           // Top-left
            sum += img[(2 * y * width + 2 * x + 1)];       // Top-right
            sum += img[((2 * y + 1) * width + 2 * x)];     // Bottom-left
            sum += img[((2 * y + 1) * width + 2 * x + 1)]; // Bottom-right
            downsampled[y * newWidth + x] = static_cast<unsigned char>(sum / 4);
        }
    }
    return downsampled;
}

std::vector<unsigned char> applyBoxBlur(const std::vector<unsigned char> &img, int width, int height, int channels)
{
    std::vector<unsigned char> blurred(width * height * channels);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            for (int c = 0; c < channels; ++c)
            {
                int sum = 0;
                int count = 0;

                // Loop over a 3x3 grid centered at (x, y)
                for (int ky = -1; ky <= 1; ++ky)
                {
                    for (int kx = -1; kx <= 1; ++kx)
                    {
                        int nx = clamp(x + kx, 0, width - 1);  // Clamp x to [0, width-1]
                        int ny = clamp(y + ky, 0, height - 1); // Clamp y to [0, height-1]

                        sum += img[(ny * width + nx) * channels + c];
                        count++;
                    }
                }

                blurred[(y * width + x) * channels + c] = sum / count;
            }
        }
    }
    return blurred;
}

std::vector<unsigned char> downsample(const std::vector<unsigned char> &img, int width, int height, int channels)
{
    int newWidth = width / 2;
    int newHeight = height / 2;
    std::vector<unsigned char> downsampled(newWidth * newHeight * channels);

    for (int y = 0; y < newHeight; ++y)
    {
        for (int x = 0; x < newWidth; ++x)
        {
            for (int c = 0; c < channels; ++c)
            {
                int sum = 0;
                sum += img[(2 * y * width + 2 * x) * channels + c];
                sum += img[(2 * y * width + 2 * x + 1) * channels + c];
                sum += img[((2 * y + 1) * width + 2 * x) * channels + c];
                sum += img[((2 * y + 1) * width + 2 * x + 1) * channels + c];

                downsampled[(y * newWidth + x) * channels + c] = sum / 4;
            }
        }
    }
    return downsampled;
}

std::vector<unsigned char> downsampleMask(const std::vector<unsigned char> &img, int width, int height)
{
    int newWidth = width / 2;
    int newHeight = height / 2;
    std::vector<unsigned char> downsampled(newWidth * newHeight, 0);

    for (int y = 0; y < newHeight; ++y)
    {
        for (int x = 0; x < newWidth; ++x)
        {
            downsampled[y * newWidth + x] = img[(2 * y) * width + ((2 * x) + 1)];
        }
    }
    return downsampled;
}

std::vector<unsigned char> upsample(const std::vector<unsigned char> &img, int width, int height, int channels)
{
    int newWidth = width * 2;
    int newHeight = height * 2;
    std::vector<unsigned char> upsampled(newWidth * newHeight * channels);

    for (int y = 0; y < newHeight; ++y)
    {
        for (int x = 0; x < newWidth; ++x)
        {
            for (int c = 0; c < channels; ++c)
            {
                // Original pixel positions
                float srcX = x / 2.0f;
                float srcY = y / 2.0f;

                // Get the integer and fractional parts
                int x0 = static_cast<int>(srcX);
                int y0 = static_cast<int>(srcY);
                float fx = srcX - x0;
                float fy = srcY - y0;

                // Clamp x0, y0 to be within bounds
                x0 = std::min(x0, width - 2);
                y0 = std::min(y0, height - 2);

                // Get the four surrounding pixels
                unsigned char p00 = img[(y0 * width + x0) * channels + c];
                unsigned char p01 = img[(y0 * width + (x0 + 1)) * channels + c];
                unsigned char p10 = img[((y0 + 1) * width + x0) * channels + c];
                unsigned char p11 = img[((y0 + 1) * width + (x0 + 1)) * channels + c];

                // Bilinear interpolation
                unsigned char interpolated = static_cast<unsigned char>(
                    (1 - fx) * (1 - fy) * p00 +
                    fx * (1 - fy) * p01 +
                    (1 - fx) * fy * p10 +
                    fx * fy * p11);

                upsampled[(y * newWidth + x) * channels + c] = interpolated;
            }
        }
    }
    return upsampled;
}

std::vector<unsigned char> computeLaplacian(const std::vector<unsigned char> &original,
                                            const std::vector<unsigned char> &upsampled, int width, int height)
{
    std::vector<unsigned char> laplacian(width * height * CHANNELS);

    for (size_t i = 0; i < laplacian.size(); ++i)
    {
        laplacian[i] = clamp(upsampled[i] - original[i], 0, 255);
    }
    return laplacian;
}

std::vector<unsigned char> laplace_blending(const std::vector<std::vector<unsigned char>> &imgBufs,
                                            const std::vector<std::vector<unsigned char>> &masks,
                                            int width, int height, int levels = 5)
{
    int numImages = imgBufs.size();

    std::vector<std::vector<std::vector<unsigned char>>> imgLaplacians(numImages);
    std::vector<std::vector<std::vector<unsigned char>>> maskGaussians(numImages);

    std::vector<int> widthLevels(levels + 1);
    std::vector<int> heightLevels(levels + 1);
    widthLevels[0] = width;
    heightLevels[0] = height;

    for (int i = 1; i <= levels; ++i)
    {
        widthLevels[i] = widthLevels[i - 1] / 2;
        heightLevels[i] = heightLevels[i - 1] / 2;
    }

    for (int i = 0; i < numImages; ++i)
    {
        std::vector<unsigned char> current = imgBufs[i];

        for (int j = 0; j < levels; ++j)
        {
            auto down = downsample(current, widthLevels[j], heightLevels[j], CHANNELS);
            auto up = upsample(down, widthLevels[j] / 2, heightLevels[j] / 2, CHANNELS);

            imgLaplacians[i].push_back(computeLaplacian(current, up, widthLevels[j], heightLevels[j]));
            current = down;
        }
        imgLaplacians[i].push_back(current);
    }

    for (int i = 0; i < masks.size(); ++i)
    {
        std::vector<unsigned char> current = masks[i];
        for (int j = 0; j < levels; ++j)
        {
            maskGaussians[i].push_back(current);
            current = downsample(current, widthLevels[j], heightLevels[j], 1);
        }
        maskGaussians[i].push_back(current);
    }

    std::vector<std::vector<unsigned char>> blendedPyramid(levels + 1);
    for (int level = 0; level <= levels; ++level)
    {
        int levelWidth = widthLevels[level];
        int levelHeight = heightLevels[level];
        std::vector<unsigned char> blend(levelWidth * levelHeight * CHANNELS, 0);

        for (int i = 0; i < numImages; ++i)
        {
            for (int j = 0; j < blend.size(); ++j)
            {
                int maskIndex = j / CHANNELS;
                int imgVal = imgLaplacians[i][level][j];
                int maskVal = maskGaussians[0][level][maskIndex] / 255;


                if (i % 2 == 1)
                    blend[j] += (imgVal * maskVal);
                else
                    blend[j] += (imgVal * (1 - maskVal));
            }
        }

        blendedPyramid[level] = blend;
    }

    std::vector<unsigned char> blendedImage = blendedPyramid[levels];
    for (int level = levels - 1; level > 0; --level)
    {
        blendedImage = upsample(blendedImage, widthLevels[level], heightLevels[level], CHANNELS);
        for (size_t i = 0; i < blendedImage.size(); ++i)
            blendedImage[i] = clamp(blendedImage[i] + blendedPyramid[level - 1][i], 0, 255);
    }
    return blendedImage;
}