#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "src/MultiscaleLocalization.h"

static void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(1);
    }
}

static LocalizationCandidate candidate(const std::string &payload,
                                       double minX, double minY,
                                       double maxX, double maxY,
                                       double rms, int attempt)
{
    LocalizationCandidate value;
    value.payload = payload;
    value.bounds = OriginalImageBounds(minX, minY, maxX, maxY);
    value.normalizedCarrierRms = rms;
    value.modelCloseFraction = 0.95;
    value.carrierTemplateGap = 10;
    value.carrierTemplateLoss = 100;
    value.attemptIndex = attempt;
    return value;
}

int main()
{
    std::vector<LocalizationScaleAttempt> small = productionLocalizationScales(512, 512);
    require(small.size() == 1 && small[0].scaleX == 1.0 && small[0].scaleY == 1.0,
            "small published corpora remain native-only");

    std::vector<LocalizationScaleAttempt> camera = productionLocalizationScales(4032, 3024);
    require(camera.size() == 3, "large camera frame has two tiers and a bounded native fallback");
    require(camera[0].width == 1320 && camera[0].height == 990,
            "first camera tier targets a 1320-pixel long edge");
    require(camera[1].width == 1600 && camera[1].height == 1200,
            "fallback camera tier targets a 1600-pixel long edge");
    require(camera[2].width == 4032 && camera[2].height == 3024 &&
            camera[2].scaleX == 1.0 && camera[2].scaleY == 1.0,
            "published native behavior remains the final fallback");

    OriginalImageBounds restored = mapBoundsToOriginal(33.0, 66.0, 330.0, 264.0,
                                                       0.33, 0.33);
    require(std::fabs(restored.minX - 100.0) < 1e-9 &&
            std::fabs(restored.minY - 200.0) < 1e-9 &&
            std::fabs(restored.maxX - 1000.0) < 1e-9 &&
            std::fabs(restored.maxY - 800.0) < 1e-9,
            "candidate bounds restore exactly to original-image coordinates");

    LocalizationCandidate first = candidate("000000000000000000000000000000000001",
                                            100, 100, 300, 300, 0.02, 0);
    LocalizationCandidate same = candidate(first.payload, 108, 104, 296, 304, 0.01, 1);
    std::vector<LocalizationCandidate> merged =
        deduplicateLocalizationCandidates({first, same});
    require(merged.size() == 1 && merged[0].attemptIndex == 1,
            "geometric duplicate keeps the strongest deterministic result");

    LocalizationCandidate conflict = same;
    conflict.payload = "100000000000000000000000000000000000";
    require(deduplicateLocalizationCandidates({first, conflict}).empty(),
            "overlapping candidates with conflicting payloads reject safely");

    LocalizationCandidate far = candidate(first.payload, 500, 500, 700, 700, 0.01, 1);
    require(deduplicateLocalizationCandidates({first, far}).size() == 2,
            "payload agreement without geometric overlap never deduplicates carriers");

    std::cout << "PASS: bounded scale policy, coordinate restoration, and geometric deduplication"
              << std::endl;
    return 0;
}
