#include "MultiscaleLocalization.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace
{
const int MultiscaleMinimumLongEdge = 2400;
const int FirstTargetLongEdge = 1320;
const int SecondTargetLongEdge = 1600;

LocalizationScaleAttempt targetLongEdge(int sourceWidth, int sourceHeight,
                                        int target, const std::string &label)
{
    int sourceLongEdge = std::max(sourceWidth, sourceHeight);
    double nominalScale = static_cast<double>(target) / sourceLongEdge;
    int width = std::max(1, static_cast<int>(sourceWidth * nominalScale + 0.5));
    int height = std::max(1, static_cast<int>(sourceHeight * nominalScale + 0.5));
    return LocalizationScaleAttempt(static_cast<double>(width) / sourceWidth,
                                    static_cast<double>(height) / sourceHeight,
                                    width, height, label);
}

bool strongerCandidate(const LocalizationCandidate &first,
                       const LocalizationCandidate &second)
{
    if (first.normalizedCarrierRms != second.normalizedCarrierRms)
        return first.normalizedCarrierRms < second.normalizedCarrierRms;
    if (first.modelCloseFraction != second.modelCloseFraction)
        return first.modelCloseFraction > second.modelCloseFraction;
    if (first.carrierTemplateGap != second.carrierTemplateGap)
        return first.carrierTemplateGap > second.carrierTemplateGap;
    if (first.carrierTemplateLoss != second.carrierTemplateLoss)
        return first.carrierTemplateLoss < second.carrierTemplateLoss;
    if (first.attemptIndex != second.attemptIndex)
        return first.attemptIndex < second.attemptIndex;
    return first.featureIndex < second.featureIndex;
}
}

std::vector<LocalizationScaleAttempt> productionLocalizationScales(int width, int height)
{
    std::vector<LocalizationScaleAttempt> attempts;
    int longEdge = std::max(width, height);
    if (longEdge < MultiscaleMinimumLongEdge)
    {
        attempts.push_back(LocalizationScaleAttempt(1.0, 1.0, width, height, "native"));
        return attempts;
    }
    attempts.push_back(targetLongEdge(width, height, FirstTargetLongEdge, "long-edge-1320"));
    attempts.push_back(targetLongEdge(width, height, SecondTargetLongEdge, "long-edge-1600"));
    // Preserve the published large-image behavior as a bounded final fallback.
    // It runs only when both empirically favorable localization tiers reject.
    attempts.push_back(LocalizationScaleAttempt(1.0, 1.0, width, height, "native-fallback"));
    return attempts;
}

OriginalImageBounds mapBoundsToOriginal(double minX, double minY,
                                        double maxX, double maxY,
                                        double scaleX, double scaleY)
{
    if (!(scaleX > 0.0) || !(scaleY > 0.0))
        return OriginalImageBounds();
    return OriginalImageBounds(minX / scaleX, minY / scaleY,
                               maxX / scaleX, maxY / scaleY);
}

double boundsIntersectionOverMinimumArea(const OriginalImageBounds &first,
                                         const OriginalImageBounds &second)
{
    double firstWidth = std::max(0.0, first.maxX - first.minX);
    double firstHeight = std::max(0.0, first.maxY - first.minY);
    double secondWidth = std::max(0.0, second.maxX - second.minX);
    double secondHeight = std::max(0.0, second.maxY - second.minY);
    double minimumArea = std::min(firstWidth * firstHeight, secondWidth * secondHeight);
    if (!(minimumArea > 0.0))
        return 0.0;
    double intersectionWidth = std::max(
        0.0, std::min(first.maxX, second.maxX) - std::max(first.minX, second.minX));
    double intersectionHeight = std::max(
        0.0, std::min(first.maxY, second.maxY) - std::max(first.minY, second.minY));
    return intersectionWidth * intersectionHeight / minimumArea;
}

bool samePhysicalCarrier(const LocalizationCandidate &first,
                         const LocalizationCandidate &second)
{
    double firstWidth = first.bounds.maxX - first.bounds.minX;
    double firstHeight = first.bounds.maxY - first.bounds.minY;
    double secondWidth = second.bounds.maxX - second.bounds.minX;
    double secondHeight = second.bounds.maxY - second.bounds.minY;
    if (!(firstWidth > 0.0) || !(firstHeight > 0.0) ||
        !(secondWidth > 0.0) || !(secondHeight > 0.0))
        return false;
    double firstCenterX = (first.bounds.minX + first.bounds.maxX) * 0.5;
    double firstCenterY = (first.bounds.minY + first.bounds.maxY) * 0.5;
    double secondCenterX = (second.bounds.minX + second.bounds.maxX) * 0.5;
    double secondCenterY = (second.bounds.minY + second.bounds.maxY) * 0.5;
    double minimumDiagonal = std::min(std::hypot(firstWidth, firstHeight),
                                     std::hypot(secondWidth, secondHeight));
    double centerDistance = std::hypot(firstCenterX - secondCenterX,
                                       firstCenterY - secondCenterY);
    return boundsIntersectionOverMinimumArea(first.bounds, second.bounds) >= 0.65 &&
           centerDistance <= 0.25 * minimumDiagonal;
}

std::vector<LocalizationCandidate> deduplicateLocalizationCandidates(
    const std::vector<LocalizationCandidate> &candidates)
{
    std::vector<bool> consumed(candidates.size(), false);
    std::vector<LocalizationCandidate> result;
    for (size_t index = 0; index < candidates.size(); index++)
    {
        if (consumed[index])
            continue;
        consumed[index] = true;
        std::vector<size_t> group(1, index);
        for (size_t other = index + 1; other < candidates.size(); other++)
        {
            if (!consumed[other] && samePhysicalCarrier(candidates[index], candidates[other]))
            {
                consumed[other] = true;
                group.push_back(other);
            }
        }
        bool conflict = false;
        size_t strongest = group[0];
        for (size_t member : group)
        {
            if (candidates[member].payload != candidates[group[0]].payload)
                conflict = true;
            if (strongerCandidate(candidates[member], candidates[strongest]))
                strongest = member;
        }
        if (!conflict)
            result.push_back(candidates[strongest]);
    }
    return result;
}
