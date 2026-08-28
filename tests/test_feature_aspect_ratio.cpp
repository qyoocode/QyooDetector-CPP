#include <iostream>
#include <string>

#include "src/Feature.h"


bool verbose = false;

void logVerbose(const std::string &)
{
}


static Feature rectangularFeature(int width, int height)
{
    Feature feature;
    feature.addPointEnd(100, 100);
    feature.addPointEnd(100 + width, 100);
    feature.addPointEnd(100 + width, 100 + height);
    feature.addPointEnd(100, 100 + height);
    return feature;
}


static bool expectValidity(const char *name, int width, int height, bool expected)
{
    Feature feature = rectangularFeature(width, height);
    feature.checkSizeAndPosition(1000, 1000);
    if (feature.valid == expected)
        return true;

    std::cerr << "FAIL " << name << ": " << width << "x" << height
              << " expected valid=" << expected << " observed valid=" << feature.valid << std::endl;
    return false;
}


int main()
{
    int failures = 0;
    failures += !expectValidity("square Qyoo-like candidate", 300, 300, true);
    failures += !expectValidity("moderately rectangular Qyoo-like candidate", 300, 225, true);
    failures += !expectValidity("wide implausible candidate", 600, 100, false);
    failures += !expectValidity("skinny implausible candidate", 100, 600, false);
    failures += !expectValidity("exact 2:1 boundary", 400, 200, true);
    failures += !expectValidity("just inside 2:1 boundary", 399, 200, true);
    failures += !expectValidity("just outside 2:1 boundary", 401, 200, false);
    failures += !expectValidity("zero-width candidate", 0, 300, false);

    if (failures != 0) {
        std::cerr << failures << " aspect-ratio expectation(s) failed" << std::endl;
        return 1;
    }
    std::cout << "PASS: intended 1:2 aspect-ratio boundary is enforced" << std::endl;
    return 0;
}
