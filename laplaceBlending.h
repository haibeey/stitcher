
#include <vector>

std::vector<unsigned char> laplace_blending(const std::vector<std::vector<unsigned char>>& imgBufs,
                                            const std::vector<std::vector<unsigned char>>& masks,
                                            int width, int height, int levels = 5);
                                        

std::vector<unsigned char> upsample(const std::vector<unsigned char> &img, int width, int height, int channels);