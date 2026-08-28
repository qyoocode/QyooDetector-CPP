#include <cassert>
#include <iostream>

#include "../src/Feature.h"

bool verbose = false;
void logVerbose(const std::string &) { }

static Feature rectangle(int width, int height)
{
    Feature feature;
    feature.addPointEnd(0, 0);
    feature.addPointEnd(width, 0);
    feature.addPointEnd(width, height);
    feature.addPointEnd(0, height);
    return feature;
}

int main()
{
    Feature qualified = rectangle(128, 128);
    qualified.checkSizeAndPosition(2048, 2048);
    assert(qualified.valid);
    assert(qualified.sizeCheckPassed);

    Feature belowAbsoluteFloor = rectangle(127, 200);
    belowAbsoluteFloor.checkSizeAndPosition(2048, 2048);
    assert(!belowAbsoluteFloor.valid);
    assert(belowAbsoluteFloor.rejectionReason == FeatureTooSmall);

    Feature belowRelativeFloor = rectangle(128, 128);
    belowRelativeFloor.checkSizeAndPosition(4096, 4096);
    assert(!belowRelativeFloor.valid);
    assert(belowRelativeFloor.rejectionReason == FeatureTooSmall);

    Feature historicalRelativeGate = rectangle(100, 100);
    historicalRelativeGate.checkSizeAndPosition(512, 512);
    assert(historicalRelativeGate.valid);
    assert(historicalRelativeGate.sizeCheckPassed);

    std::cout << "PASS: large-frame qualification requires both the 0.3% relative and 128 px absolute floors\n";
    return 0;
}
