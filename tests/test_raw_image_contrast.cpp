#include <iostream>

#include "src/RawImage.h"


static bool testUniformLevels()
{
    int failures = 0;
    for (int level = 0; level <= 255; ++level) {
        RawImageGray8 image(7, 5);
        for (int y = 0; y < image.getSizeY(); ++y)
            for (int x = 0; x < image.getSizeX(); ++x)
                image.getPixel(x, y) = static_cast<unsigned char>(level);

        image.runContrast();
        for (int y = 0; y < image.getSizeY(); ++y)
            for (int x = 0; x < image.getSizeX(); ++x)
                if (image.getPixel(x, y) != level)
                    ++failures;
    }
    if (failures != 0)
        std::cerr << "FAIL: " << failures << " uniform pixels changed during undefined zero-range scaling" << std::endl;
    return failures == 0;
}


static bool testNonUniformScaling()
{
    RawImageGray8 image(2, 1);
    image.getPixel(0, 0) = 10;
    image.getPixel(1, 0) = 20;
    image.runContrast();
    if (image.getPixel(0, 0) == 0 && image.getPixel(1, 0) == 255)
        return true;
    std::cerr << "FAIL: ordinary contrast scaling changed; observed "
              << static_cast<int>(image.getPixel(0, 0)) << ","
              << static_cast<int>(image.getPixel(1, 0)) << std::endl;
    return false;
}


int main()
{
    if (!testUniformLevels() || !testNonUniformScaling())
        return 1;
    std::cout << "PASS: uniform images remain deterministic and ordinary scaling is preserved" << std::endl;
    return 0;
}
