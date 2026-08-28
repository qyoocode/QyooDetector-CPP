#include <iostream>
#include <string>
#include <vector>

#include "src/FeatureDetector.cpp"


bool verbose = false;


int main()
{
    RawImageGray8 image(11, 11);
    for (int y = 0; y < image.getSizeY(); ++y)
        for (int x = 0; x < image.getSizeX(); ++x)
            image.getPixel(x, y) = 100;

    ConvolutionFilterInt *filter = MakeRadiusFilter(11, 5);
    std::vector<int> samples(filter->getSize() * filter->getSize());
    filter->processPixel(&image, 5, 5, samples.data());

    int sentinelCount = 0;
    for (int sample : samples)
        sentinelCount += sample == -1;
    int observed = calcAvgPixel(&image, 5, 5, filter);
    int validCount = filter->getFact();
    delete filter;

    if (sentinelCount == 0) {
        std::cerr << "FAIL: radius filter did not expose excluded-sample sentinels" << std::endl;
        return 1;
    }
    if (observed != 100) {
        std::cerr << "FAIL: constant background average expected 100, observed " << observed
                  << "; " << sentinelCount << " -1 sentinels contaminated " << validCount
                  << " valid samples" << std::endl;
        return 1;
    }
    std::cout << "PASS: " << sentinelCount
              << " excluded radius samples do not affect the background average" << std::endl;
    return 0;
}
