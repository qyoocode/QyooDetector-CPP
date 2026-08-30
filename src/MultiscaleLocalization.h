#pragma once

#include <string>
#include <vector>

struct LocalizationScaleAttempt
{
    LocalizationScaleAttempt(double inScaleX = 1.0, double inScaleY = 1.0,
                             int inWidth = 0, int inHeight = 0,
                             const std::string &inLabel = "native")
        : scaleX(inScaleX), scaleY(inScaleY), width(inWidth), height(inHeight),
          label(inLabel) { }

    double scaleX;
    double scaleY;
    int width;
    int height;
    std::string label;
};

struct OriginalImageBounds
{
    OriginalImageBounds(double inMinX = 0.0, double inMinY = 0.0,
                        double inMaxX = 0.0, double inMaxY = 0.0)
        : minX(inMinX), minY(inMinY), maxX(inMaxX), maxY(inMaxY) { }

    double minX;
    double minY;
    double maxX;
    double maxY;
};

struct LocalizationCandidate
{
    std::string payload;
    OriginalImageBounds bounds;
    double modelCloseFraction = 0.0;
    double normalizedCarrierRms = 0.0;
    int carrierTemplateLoss = 0;
    int carrierTemplateGap = 0;
    bool requiresRasterConfirmation = false;
    int attemptIndex = 0;
    int featureIndex = 0;
};

// Production uses two deterministic LANCZOS tiers on large camera frames and
// a bounded native fallback only when both reject. Smaller images retain the
// published native path exactly.
std::vector<LocalizationScaleAttempt> productionLocalizationScales(int width, int height);

OriginalImageBounds mapBoundsToOriginal(double minX, double minY,
                                        double maxX, double maxY,
                                        double scaleX, double scaleY);

double boundsIntersectionOverMinimumArea(const OriginalImageBounds &first,
                                         const OriginalImageBounds &second);
bool samePhysicalCarrier(const LocalizationCandidate &first,
                         const LocalizationCandidate &second);

// Geometry establishes duplicate groups. Payload equality alone never does.
// A geometrically duplicate group with conflicting payloads is rejected.
std::vector<LocalizationCandidate> deduplicateLocalizationCandidates(
    const std::vector<LocalizationCandidate> &candidates);
