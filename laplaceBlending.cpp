#include <turbojpeg.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <string>

#include "utils.h"
#include "laplaceBlending.h"
#include "edJpeg.h"

const int CHANNELS = 3;
const int GAUSSIAN_SIZE = 5;
const float GAUSSIAN_SIGMA = 1.0f;

Image applyGaussianFilter(Image img)
{
    std::vector<unsigned char> blurred(img.width * img.height * img.channels);
    int kernelSize = 3;
    float kernel[3][3] = {
        {1 / 16.0f, 2 / 16.0f, 1 / 16.0f},
        {2 / 16.0f, 4 / 16.0f, 2 / 16.0f},
        {1 / 16.0f, 2 / 16.0f, 1 / 16.0f}};

    for (int y = 0; y < img.height; ++y)
    {
        for (int x = 0; x < img.width; ++x)
        {
            for (int c = 0; c < img.channels; ++c) // Process each channel separately
            {
                float sum = 0.0f;
                for (int ky = -1; ky <= 1; ++ky)
                {
                    for (int kx = -1; kx <= 1; ++kx)
                    {
                        int nx = clamp(x + kx, 0, img.width - 1);
                        int ny = clamp(y + ky, 0, img.height - 1);
                        sum += img.data[(ny * img.width + nx) * img.channels + c] * kernel[ky + 1][kx + 1];
                    }
                }
                blurred[(y * img.width + x) * img.channels + c] = static_cast<unsigned char>(sum);
            }
        }
    }
    return Image(blurred, img.width, img.height, img.channels);
}

Image applyBoxBlur(Image img)
{
    std::vector<unsigned char> blurred(img.width * img.height * img.channels);

    for (int y = 0; y < img.height; ++y)
    {
        for (int x = 0; x < img.width; ++x)
        {
            for (int c = 0; c < img.channels; ++c)
            {
                int sum = 0;
                int count = 0;

                for (int ky = -1; ky <= 1; ++ky)
                {
                    for (int kx = -1; kx <= 1; ++kx)
                    {
                        int nx = clamp(x + kx, 0, img.width - 1);  // Clamp x to [0, width-1]
                        int ny = clamp(y + ky, 0, img.height - 1); // Clamp y to [0, height-1]

                        sum += img.data[(ny * img.width + nx) * img.channels + c];
                        count++;
                    }
                }

                blurred[(y * img.width + x) * img.channels + c] = sum / count;
            }
        }
    }
    return Image(blurred, img.width, img.height, img.channels);
}

void downSampleOperation(int startRow, int endRow, int newWidth, int newHeight, const Image &img, std::vector<unsigned char> &downsampled)
{
    for (int y = startRow; y < endRow; ++y)
    {
        for (int x = 0; x < newWidth; ++x)
        {
            for (int c = 0; c < img.channels; ++c)
            {
                int sum = 0;
                sum += img.data[(2 * y * img.width + 2 * x) * img.channels + c];
                sum += img.data[(2 * y * img.width + 2 * x + 1) * img.channels + c];
                sum += img.data[((2 * y + 1) * img.width + 2 * x) * img.channels + c];
                sum += img.data[((2 * y + 1) * img.width + 2 * x + 1) * img.channels + c];

                downsampled[(y * newWidth + x) * img.channels + c] = sum / 4;
            }
        }
    }
}
void maskDownSampleOperation(int startRow, int endRow, int newWidth, int newHeight, const Image &img, std::vector<unsigned char> &downsampled)
{
    for (int y = startRow; y < endRow; ++y)
    {
        for (int x = 0; x < newWidth; ++x)
            downsampled[y * newWidth + x] = img.data[(2 * y) * img.width + ((2 * x) + 1)];
    }
}

Image downsample(const Image &img)
{
    int newWidth = img.width / 2;
    int newHeight = img.height / 2;
    std::vector<unsigned char> downsampled(newWidth * newHeight * img.channels);

    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0)
        numThreads = 1;

    int rowsPerThread = newHeight / numThreads;
    int remainingRows = newHeight % numThreads;

    std::vector<std::thread> threads;
    int startRow = 0;

    for (unsigned int i = 0; i < numThreads; ++i)
    {
        int endRow = startRow + rowsPerThread + (remainingRows > 0 ? 1 : 0);
        if (remainingRows > 0)
            --remainingRows;

        if (img.channels == 3)
            threads.emplace_back(downSampleOperation, startRow, endRow, newWidth, newHeight, std::cref(img), std::ref(downsampled));
        else
            threads.emplace_back(maskDownSampleOperation, startRow, endRow, newWidth, newHeight, std::cref(img), std::ref(downsampled));

        startRow = endRow;
    }

    for (std::thread &t : threads)
    {
        t.join();
    }

    return Image(downsampled, newWidth, newHeight, img.channels);
}

Image upsample(Image img)
{
    int newWidth = img.width * 2;
    int newHeight = img.height * 2;
    std::vector<unsigned char> upsampled(newWidth * newHeight * img.channels);

    int numThreads = std::thread::hardware_concurrency();
    numThreads = std::max(1, numThreads);

    auto worker = [&](int startRow, int endRow)
    {
        for (int y = startRow; y < endRow; ++y)
        {
            for (int x = 0; x < newWidth; ++x)
            {
                for (int c = 0; c < img.channels; ++c)
                {
                    float srcX = x / 2.0f;
                    float srcY = y / 2.0f;

                    int x0 = static_cast<int>(srcX);
                    int y0 = static_cast<int>(srcY);
                    float fx = srcX - x0;
                    float fy = srcY - y0;

                    x0 = std::min(x0, img.width - 2);
                    y0 = std::min(y0, img.height - 2);

                    unsigned char p00 = img.data[(y0 * img.width + x0) * img.channels + c];
                    unsigned char p01 = img.data[(y0 * img.width + (x0 + 1)) * img.channels + c];
                    unsigned char p10 = img.data[((y0 + 1) * img.width + x0) * img.channels + c];
                    unsigned char p11 = img.data[((y0 + 1) * img.width + (x0 + 1)) * img.channels + c];

                    unsigned char interpolated = static_cast<unsigned char>(
                        (1 - fx) * (1 - fy) * p00 +
                        fx * (1 - fy) * p01 +
                        (1 - fx) * fy * p10 +
                        fx * fy * p11);

                    upsampled[(y * newWidth + x) * img.channels + c] = interpolated;
                }
            }
        }
    };

    std::vector<std::thread> threads;
    int rowsPerThread = newHeight / numThreads;
    int remainingRows = newHeight % numThreads;

    for (int i = 0; i < numThreads; ++i)
    {
        int startRow = i * rowsPerThread;
        int endRow = startRow + rowsPerThread + (i == numThreads - 1 ? remainingRows : 0);
        threads.emplace_back(worker, startRow, endRow);
    }

    for (auto &t : threads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    return Image(upsampled, newWidth, newHeight, img.channels);
}

Image computeLaplacian(Image original, Image upsampled)
{
    std::vector<unsigned char> laplacian(original.width * original.height * CHANNELS);
    int numThreads = std::thread::hardware_concurrency();
    numThreads = std::max(1, numThreads);

    auto worker = [&](int startIndex, int endIndex)
    {
        for (int i = startIndex; i < endIndex; ++i)
        {
            laplacian[i] = clamp(original.data[i] - upsampled.data[i], 0, 255);
        }
    };

    std::vector<std::thread> threads;
    int rowsPerThread = original.size() / numThreads;
    int remainingRows = original.size() % numThreads;

    for (int i = 0; i < numThreads; ++i)
    {
        int startRow = i * rowsPerThread;
        int endRow = startRow + rowsPerThread + (i == numThreads - 1 ? remainingRows : 0);
        threads.emplace_back(worker, startRow, endRow);
    }

    for (auto &t : threads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    return Image(laplacian, original.width, original.height, original.channels);
}

void Blender::feed(Image img, Image maskImg, Point tl)
{
    auto start = std::chrono::high_resolution_clock::now();
    int gap = 3 * (1 << num_bands);
    Point tl_new(std::max(output_size.x, tl.x - gap),
                 std::max(output_size.y, tl.y - gap));
    Point br_new(std::min(output_size.br().x, tl.x + img.width + gap),
                 std::min(output_size.br().y, tl.y + img.height + gap));

    tl_new.x = output_size.x + (((tl_new.x - output_size.x) >> num_bands) << num_bands);
    tl_new.y = output_size.y + (((tl_new.y - output_size.y) >> num_bands) << num_bands);
    int width = br_new.x - tl_new.x;
    int height = br_new.y - tl_new.y;
    width += ((1 << num_bands) - width % (1 << num_bands)) % (1 << num_bands);
    height += ((1 << num_bands) - height % (1 << num_bands)) % (1 << num_bands);
    br_new.x = tl_new.x + width;
    br_new.y = tl_new.y + height;
    int dy = std::max(br_new.y - output_size.br().y, 0);
    int dx = std::max(br_new.x - output_size.br().x, 0);
    tl_new.x -= dx;
    br_new.x -= dx;
    tl_new.y -= dy;
    br_new.y -= dy;

    int top = tl.y - tl_new.y;
    int left = tl.x - tl_new.x;
    int bottom = br_new.y - tl.y - img.height;
    int right = br_new.x - tl.x - img.width;

    addBorderToImage(img.data, img.width, img.height, top, bottom, left, right, CHANNELS, BORDER_REFLECT);
    addBorderToImage(maskImg.data, maskImg.width, maskImg.height, top, bottom, left, right, 1, BORDER_CONSTANT);

    // std::cout << top << " " << left << " " << bottom << " " << right << " " << img.height % 2 << " lol \n";

    std::vector<Image> imgLaplacians;
    Image current = img;

    for (int j = 0; j < num_bands; ++j)
    {
        Image down = downsample(current);
        Image up = upsample(down);

        imgLaplacians.push_back(computeLaplacian(current, up));
        // imgLaplacians.push_back(current);

        current = down;
    }
    imgLaplacians.push_back(current);

    Image mask = maskImg;
    std::vector<Image> maskGaussian;
    for (int j = 0; j < num_bands; ++j)
    {
        maskGaussian.push_back(mask);
        mask = downsample(mask);
    }
    maskGaussian.push_back(mask);

    int y_tl = tl_new.y - output_size.y;
    int y_br = br_new.y - output_size.y;
    int x_tl = tl_new.x - output_size.x;
    int x_br = br_new.x - output_size.x;

    for (int level = 0; level <= num_bands; ++level)
    {
        // std::string filename = "merge" + std::to_string(level) + ".jpg";
        // if (compressJPEG(filename.c_str(), imgLaplacians[level].data.data(), imgLaplacians[level].width, imgLaplacians[level].height, 100))
        // {
        //     // std::cout << " Image saved " << std::endl;
        // }

        int levelWidth = imgLaplacians[level].width;
        int levelHeight = imgLaplacians[level].height;
        std::vector<unsigned char> blend(levelWidth * levelHeight * CHANNELS, 0);

        int scaledX = std::max(0, x_tl);
        int scaledY = std::max(0, y_tl);

        int outLevelWidth = outWidthLevels[level];
        int outLevelHeight = outHeightLevels[level];

        int size = outLevelHeight * outLevelWidth * CHANNELS;

        int rows = (y_br - y_tl);
        int cols = (x_br - x_tl);

        auto worker = [&](int startRow, int endRow)
        {
            for (int k = startRow; k < endRow; k++)
            {
                for (int i = 0; i < cols; i++)
                {
                    for (int z = 0; z < CHANNELS; z++)
                    {
                        // The current image index is essentially the  width of images consumed multiple by channels + current width span + channel index
                        int imgIndex = ((i + (k * levelWidth)) * CHANNELS) + z;
                        int maskIndex = imgIndex / CHANNELS;
                        if (imgIndex < imgLaplacians[level].size() && maskIndex < maskGaussian[level].size())
                        {
                            int outlevelIndex = (((i + scaledX) + ((k + scaledY) * outLevelWidth)) * CHANNELS) + z;
                            int outMasklevelIndex = (((i + scaledX) + ((k + scaledY) * outLevelWidth)));

                            int imgVal = imgLaplacians[level].data[imgIndex];
                            int maskVal = maskGaussian[level].data[maskIndex] / 255;

                            if (outlevelIndex < size && (i + scaledX) < outLevelWidth && (k + scaledY) < outLevelHeight)
                            {
                                out[level].data[outlevelIndex] += (imgVal * maskVal);
                                outMask[level].data[outMasklevelIndex] += maskGaussian[level].data[maskIndex];
                            }
                        }
                    }
                }
            }
        };

        std::vector<std::thread> threads;
        int rowsPerThread = rows / numThreads;
        int remainingRows = rows % numThreads;

        for (int i = 0; i < numThreads; ++i)
        {
            int startRow = i * rowsPerThread;
            int endRow = startRow + rowsPerThread + (i == numThreads - 1 ? remainingRows : 0);
            threads.emplace_back(worker, startRow, endRow);
        }

        for (auto &t : threads)
        {
            if (t.joinable())
            {
                t.join();
            }
        }

        // std::cout << " the count " << count << "\n";

        x_tl /= 2;
        y_tl /= 2;
        x_br /= 2;
        y_br /= 2;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
    std::cout << "Elapsed time for feed: " << duration.count() << " s" << std::endl;
}

void Blender::blend()
{
    auto start = std::chrono::high_resolution_clock::now();
    for (int level = 0; level < num_bands; level++)
    {
        auto worker = [&](int startRow, int endRow)
        {
            for (int y = startRow; y < endRow; ++y)
            {

                for (int x = 0; x < output_size.width; ++x)
                {

                    int maskIndex = ((x + (y * output_size.width)));
                    if (maskIndex < outMask[level].size())
                    {
                        int w = outMask[level].data[maskIndex] + 1;

                        for (int z = 0; z < CHANNELS; z++)
                        {
                            int imgIndex = ((x + (y * output_size.width)) * CHANNELS) + z;
                            if (imgIndex < out[level].size())
                            {
                                out[level].data[imgIndex] = static_cast<unsigned char>(static_cast<int>(out[level].data[imgIndex] * 256) / static_cast<int>(w));
                            }
                        }
                    }
                }
            }
        };

        std::vector<std::thread> threads;
        int rowsPerThread = output_size.height / numThreads;
        int remainingRows = output_size.height % numThreads;

        for (int i = 0; i < numThreads; ++i)
        {
            int startRow = i * rowsPerThread;
            int endRow = startRow + rowsPerThread + (i == numThreads - 1 ? remainingRows : 0);
            threads.emplace_back(worker, startRow, endRow);
        }

        for (auto &t : threads)
        {
            if (t.joinable())
            {
                t.join();
            }
        }
    }

    Image blendedImage = out[out.size() - 1];
    for (int level = out.size() - 1; level > 0; --level)
    {

        blendedImage = upsample(blendedImage);

        auto worker = [&](int startIndex, int endIndex, int outSize)
        {
            for (int i = startIndex; i < endIndex; ++i)
            {
                if (i < outSize)
                {
                    blendedImage.data[i] = clamp(blendedImage.data[i] + out[level - 1].data[i], 0, 255);
                }
            }
        };

        int outSize = out[level - 1].size();
        std::vector<std::thread> threads;
        int rowsPerThread = outSize / numThreads;
        int remainingRows = outSize % numThreads;

        for (int i = 0; i < numThreads; ++i)
        {
            int startRow = i * rowsPerThread;
            int endRow = startRow + rowsPerThread + (i == numThreads - 1 ? remainingRows : 0);
            threads.emplace_back(worker, startRow, endRow, outSize);
        }

        for (auto &t : threads)
        {
            if (t.joinable())
            {
                t.join();
            }
        }
    }

    std::cout << "blend done\n";
    out.resize(1);
    out[0] = blendedImage;
    outMask.clear();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
    std::cout << "Elapsed time for blend: " << duration.count() << " s" << std::endl;
}
