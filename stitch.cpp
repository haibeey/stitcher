

#include <vector>
#include <iostream>

#include "laplaceBlending.h"
#include "edJpeg.h"




int main() {
    int width = 640, height = 480;

    std::vector<std::vector<unsigned char>> imgBufs(2);
    std::vector<std::vector<unsigned char>> masks(2);

    decompressJPEG("files/bottom1.jpg",imgBufs[0],width,height);
    masks[0] = createMask( width,height, 0.2f,false,true);
    decompressJPEG("files/bottom2.jpg",imgBufs[1],width,height);
    masks[1] = createMask( width,height, 0.2f,true,false);


    std::vector<unsigned char> result = upsample(laplace_blending(imgBufs, masks, width, height,4),width,height,3);

    std::cout << result.size() << " " << imgBufs[0].size() << " abraham\n";

    if (compressJPEG("merge.jpg",result.data(),width,height,100)){
        std::cout << "Image saved " << std::endl;
    }

    return 0;
}

//g++-14 -o stitch -I/usr/local/include -L/usr/local/lib -lturbojpeg  stitch.cpp laplaceBlending.cpp edJpeg.cpp  && ./stitch