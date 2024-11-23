
#include <vector>
#include <cmath>
#include <thread>

const int BANDS = 5;

struct Image
{
    std::vector<unsigned char> data;
    int width;
    int height;
    int channels;

    Image() : data(), width(0), height(0), channels(0) {}

    Image(std::vector<unsigned char> _data, int _width, int _height, int _channels)
        : data(std::move(_data)), width(_width), height(_height), channels(_channels) {}

    ~Image() = default;

    Image(const Image &other)
        : data(other.data), width(other.width), height(other.height), channels(other.channels) {}

    Image &operator=(const Image &other)
    {
        if (this != &other)
        {
            data = other.data;
            width = other.width;
            height = other.height;
            channels = other.channels;
        }
        return *this;
    }

    Image(Image &&other) noexcept
        : data(std::move(other.data)), width(other.width), height(other.height), channels(other.channels) {}

    Image &operator=(Image &&other) noexcept
    {
        if (this != &other)
        {
            data = std::move(other.data);
            width = other.width;
            height = other.height;
            channels = other.channels;
        }
        return *this;
    }

    size_t size() const
    {
        return width * height * channels;
    }
};

struct Point
{
    int x;
    int y;
    Point(int _x, int _y) : x(_x), y(_y) {}

    void print()
    {
        std::cout << x << " " << y << "\n";
    }
};

struct Rect
{
    int x;
    int y;
    int width;
    int height;
    Point br()
    {
        return Point(x + width, y + height);
    }

    Rect(int _x, int _y, int _width, int _height) : x(_x), y(_y), width(_width), height(_height) {}
    Rect() : x(0), y(0), width(0), height(0) {}

    void print()
    {
        std::cout << x << " " << y << " " << width << " " << height << "\n";
    }

    Rect(const Rect &) = default;
    Rect &operator=(const Rect &) = default;
};

class Blender
{

public:
    Blender(Rect out_size, int nb = BANDS)
    {
        num_bands = nb;
        output_size = out_size;
        double max_len = static_cast<double>(std::max(output_size.width, output_size.height));
        num_bands = std::min(num_bands, static_cast<int>(ceil(std::log(max_len) / std::log(2.0))));
        output_size.width += ((1 << num_bands) - output_size.width % (1 << num_bands)) % (1 << num_bands);
        output_size.height += ((1 << num_bands) - output_size.height % (1 << num_bands)) % (1 << num_bands);
        out.resize(num_bands + 1);
        outMask.resize(num_bands + 1);
        out[0] = Image(std::vector<unsigned char>(output_size.height * output_size.width * 3, 0), output_size.width, output_size.height, 3);
        outMask[0] = Image(std::vector<unsigned char>(output_size.height * output_size.width, 0), output_size.width, output_size.height, 1);
        outHeightLevels.push_back(output_size.height);
        outWidthLevels.push_back(output_size.width);
        for (int i = 1; i <= num_bands; i++)
        {
            outWidthLevels.push_back((outWidthLevels[i - 1] + 1) / 2);
            outHeightLevels.push_back((outHeightLevels[i - 1] + 1) / 2);

            long size = outWidthLevels[i] * outHeightLevels[i];
            out[i] = Image(std::vector<unsigned char>(size * 3, 0), outWidthLevels[i], outHeightLevels[i], 3);
            outMask[i] = Image(std::vector<unsigned char>(size, 0), outWidthLevels[i], outHeightLevels[i], 1);
        }
    }

    int numBands() const { return num_bands; }
    void setNumBands(int val) { num_bands = val; }

    void feed(Image img, Image mask, Point tl);
    void blend();
    Image result()
    {
        return out[0];
    }
    int outHeight()
    {
        return output_size.height;
    }

    int outWidth()
    {
        return output_size.width;
    }

    ~Blender()
    {
        for (auto &vec : out)
        {
            vec.data.clear();
            vec.data.shrink_to_fit();
        }
        out.clear();
        out.shrink_to_fit();

        for (auto &vec : outMask)
        {
            vec.data.clear();
            vec.data.shrink_to_fit();
        }
        outMask.clear();
        outMask.shrink_to_fit();
    }

private:
    int numThreads = std::max(1,static_cast<int>(std::thread::hardware_concurrency()));
    int num_bands;
    Rect output_size;
    std::vector<int> outWidthLevels;
    std::vector<int> outHeightLevels;

    std::vector<Image> out;
    std::vector<Image> outMask;
};

Image upsample(Image img);
Image downsample(const Image &img);
Image computeLaplacian(Image original, Image upsampled);