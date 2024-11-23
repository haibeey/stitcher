

#include <vector>
#include <iostream>
#include "laplaceBlending.h"
#include "edJpeg.h"

int main()
{
    int width = 640, height = 480;

    std::vector<std::vector<unsigned char>> imgBufs(2);

    decompressJPEG("files/bottom1.jpg", imgBufs[0], width, height);
    decompressJPEG("files/bottom2.jpg", imgBufs[1], width, height);
    std::vector<unsigned char> mask1 = createMask(width, height, 0.1f, false, true);
    std::vector<unsigned char> mask2 = createMask(width, height, 0.1f, true, false);

   
    int cut = static_cast<int>(0.1f * width);
    // std::cout << width << " " << height << "\n";
    Blender blender = Blender(Rect{0, 0, (width * 2) - (2 * cut),height});
    blender.feed(Image{imgBufs[0], width, height,3}, Image{mask1, width, height,1}, Point{0, 0});
    blender.feed(Image{imgBufs[1], width, height,3}, Image{mask2, width, height,1}, Point{width - (2 * cut),0});
    blender.blend();


    Image outImage = blender.result();
    if (compressJPEG("merge.jpg", outImage.data.data(), outImage.width, outImage.height, 100))
    {
        std::cout << "Image saved " << std::endl;
    }

    return 0;
}

// g++-14 -pthread -fsanitize=address -g -o stitch -I/usr/local/include -L/usr/local/lib -lturbojpeg  stitch.cpp laplaceBlending.cpp edJpeg.cpp  && ./stitch