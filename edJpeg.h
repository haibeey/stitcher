
#include <vector>

bool decompressJPEG(const char *filename, std::vector<unsigned char> &imgBuf, int &width, int &height);
std::vector<unsigned char> convertRGBToGray(const std::vector<unsigned char> &rgbBuffer, int width, int height);
bool compressJPEG(const char *outputFilename, unsigned char *imgBuf, int width, int height, int quality = 75);
bool compressGrayscaleJPEG(const char *outputFilename, unsigned char *imgBuf, int width, int height, int quality = 75);
std::vector<unsigned char>  createMask(int width,int height, float range, bool left = false, bool right = true);