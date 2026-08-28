#include <gd.h>
#include <iostream>
#include <string>

#include "src/Geometry.h"
#include "src/RawImage.h"

static int failures = 0;

static void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << std::endl;
        failures++;
    }
}

int main()
{
    gdImagePtr image = gdImageCreateTrueColor(16, 16);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
            gdImageSetPixel(image, x, y, gdTrueColor(x * 10, y * 10, 0));

    RawImageGray8 sampled(4, 4);
    ProjectiveTransform identityPixels(16, 0, 0, 0, 16, 0, 0, 0, 1);
    require(sampled.copyFromGDImageProjective(image, identityPixels), "finite identity-scale samples");
    require(sampled.getPixel(0, 0) == 0, "origin sample");
    require(sampled.getPixel(1, 2) == 40, "normalized x maps to source x");
    require(sampled.getPixel(3, 3) == 120, "lower-right normalized sample");

    RawImageGray8 perspectiveSampled(4, 4);
    ProjectiveTransform perspective(16, 0, 0, 0, 16, 0, 0.5, 0, 1);
    require(perspectiveSampled.copyFromGDImageProjective(image, perspective), "perspective samples");
    // x=0.75 maps to 12/(1+0.375)=8.727..., rounded to source x=9.
    require(perspectiveSampled.getPixel(3, 0) == 90, "homogeneous division changes sample coordinate");

    RawImageGray8 rejected(4, 4);
    ProjectiveTransform infinity(1, 0, 0, 0, 1, 0, 0, 0, 0);
    require(!rejected.copyFromGDImageProjective(image, infinity), "infinite mapping is rejected");

    gdImageDestroy(image);
    if (failures != 0)
        return 1;
    std::cout << "PASS: projective nearest-neighbor sampling and invalid mapping rejection" << std::endl;
    return 0;
}
