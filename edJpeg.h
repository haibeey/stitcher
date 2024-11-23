
#include <vector>

enum BorderType {
    BORDER_CONSTANT,
    BORDER_REFLECT
};

bool decompressJPEG(const char *filename, std::vector<unsigned char> &imgBuf, int &width, int &height);
std::vector<unsigned char> convertRGBToGray(const std::vector<unsigned char> &rgbBuffer, int width, int height);
bool compressJPEG(const char *outputFilename, unsigned char *imgBuf, int width, int height, int quality = 75);
bool compressGrayscaleJPEG(const char *outputFilename, unsigned char *imgBuf, int width, int height, int quality = 75);
std::vector<unsigned char>  createMask(int width,int height, float range, bool left = false, bool right = true);
std::vector<unsigned char> createVerticalMask(int width, int height, float range, bool top = false, bool bottom = true);

void addBorderToImage(std::vector<unsigned char>& img, int& width, int& height, 
                      int borderTop, int borderBottom, int borderLeft, int borderRight,
                      int channels, BorderType borderType);