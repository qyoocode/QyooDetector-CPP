/*
 *  FeatureDetector.cpp
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/20/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 *  This file contains the main logic for detecting Qyoo shapes and dots
 *  in an image. It includes processing the image, detecting features,
 *  finding dots, and validating the detected Qyoo codes.
 */

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <map>
#include <sstream>

#import "FeatureDetector.h"
#import "QyooModel.h"
#include "Logger.h"

// Pixels per dot for detection
const int PixelsPerDot = 11;
const int RadianceDistMatch = 60;
const float PassRatio = .40;  // 40% coverage

// Define constants for closing distance and decimation tolerance
const float ClosedDist = 2.0f;  // Threshold distance to consider a feature closed
const float DecimateDist = 0.85f;  // Tolerance for decimating points in a feature

// Frozen exact fallbacks are within 0.40 degrees of orthogonal. The first
// observed wrong sparse-perspective fallback is 2.92 degrees away. Two degrees
// retains a measured safety margin and is independently regression-tested.
const double MaximumQualifiedAffineAngleDeviationDegrees = 2.0;

// The selected payload must be the strict, integer-valued reconstruction
// optimum. A tie is an unsupported identifier and is rejected. Larger margins
// are reported as confidence evidence, but are not treated as calibrated
// probabilities. This retains the one-pixel native development case; the
// independent deterministic sampler must still agree before acceptance.
const int MinimumCarrierTemplatePayloadGap = 1;
const double MinimumCarrierAmbiguityAmount = -0.25;
const double MaximumCarrierAmbiguityAmount = 0.25;
// At 176 fitting pixels, a 0.005 ambiguity increment moves the payload grid by
// less than one fitting pixel across the measured carrier range. The complete
// interval is still searched so alternate-payload evidence is not hidden by a
// local optimizer.
const int CarrierAmbiguitySearchSteps = 101;
// A payload loss separation below one approximate dot perimeter can be changed
// by subpixel raster phase, so Task 09B audits those fits at half-pixel offsets.
const int CarrierTemplatePhaseAuditGapPixels = 64;
// Template acceptance must be supported by the carrier structure outside every
// legal payload-dot disk. This is payload-independent evidence: at least 90%
// of those pixels must agree with the canonical circle/square carrier.
const int MaximumCarrierStructuralMismatchPercent = 10;
// A selected total-loss solution is contradicted when another member of the
// same projective class removes at least 32 structural mismatches and at least
// one quarter of the selected solution's structural error.
const int MinimumStructuralAlternativeImprovementPixels = 32;
const int MinimumStructuralAlternativeImprovementPercent = 25;
// If at least half of four independent half-pixel audits choose a different
// payload, final acceptance requires a separate raster-scale observation.
const int RasterConfirmationPhaseDisagreements = 2;

static bool refineProjectiveFromVisibleDots(RawImageGray8 *affinePatch,
                                            const ProjectiveTransform &normalizedToImage,
                                            Feature *feature)
{
    const int radius = PixelsPerDot / 2;
    std::vector<ProjectivePoint> modelPoints;
    std::vector<ProjectivePoint> imagePoints;
    QyooModel *model = QyooModel::getQyooModel();
    int backgroundTotal = 0;
    int diskCount = 0;
    for (int dy = -radius; dy <= radius; dy++)
        for (int dx = -radius; dx <= radius; dx++)
            if (dx * dx + dy * dy < radius * radius)
            {
                backgroundTotal += affinePatch->getPixel(radius + dx, radius + dy);
                diskCount++;
            }
    int background = backgroundTotal / diskCount;
    bool backgroundIsWhite = background >= 128;
    int width = affinePatch->getSizeX();
    int height = affinePatch->getSizeY();
    std::vector<unsigned char> mask(width * height, 0);
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            int value = affinePatch->getPixel(x, y);
            int contrast = abs(background - value);
            bool oppositeSide = backgroundIsWhite ? value < 160 : value > 96;
            mask[y * width + x] = contrast > RadianceDistMatch && oppositeSide;
        }

    std::vector<unsigned char> visited(width * height, 0);
    bool assigned[QYOOSIZE][QYOOSIZE] = {{false}};
    const int minimumArea = static_cast<int>(ceil(PassRatio * diskCount));
    const int maximumArea = PixelsPerDot * PixelsPerDot;
    const int neighbors[8][2] = {
        {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
        {1, 0}, {-1, 1}, {0, 1}, {1, 1}
    };
    for (int startY = 0; startY < height; startY++)
        for (int startX = 0; startX < width; startX++)
        {
            int startIndex = startY * width + startX;
            if (!mask[startIndex] || visited[startIndex])
                continue;
            std::vector<int> pending(1, startIndex);
            visited[startIndex] = 1;
            int area = 0;
            double weightedX = 0.0;
            double weightedY = 0.0;
            double totalWeight = 0.0;
            for (size_t next = 0; next < pending.size(); next++)
            {
                int index = pending[next];
                int x = index % width;
                int y = index / width;
                int weight = abs(background - affinePatch->getPixel(x, y));
                area++;
                weightedX += x * weight;
                weightedY += y * weight;
                totalWeight += weight;
                for (const auto &neighbor : neighbors)
                {
                    int otherX = x + neighbor[0];
                    int otherY = y + neighbor[1];
                    if (otherX < 0 || otherX >= width || otherY < 0 || otherY >= height)
                        continue;
                    int otherIndex = otherY * width + otherX;
                    if (mask[otherIndex] && !visited[otherIndex])
                    {
                        visited[otherIndex] = 1;
                        pending.push_back(otherIndex);
                    }
                }
            }
            if (area < minimumArea || area > maximumArea || totalWeight == 0.0)
                continue;
            double centerX = weightedX / totalWeight;
            double centerY = weightedY / totalWeight;
            double firstCenter = PixelsPerDot + PixelsPerDot / 2;
            int position = static_cast<int>(round((centerX - firstCenter) / PixelsPerDot));
            int row = static_cast<int>(round((centerY - firstCenter) / PixelsPerDot));
            if (position < 0 || position >= model->numPos() || row < 0 || row >= model->numRows() ||
                assigned[row][position])
                continue;
            double expectedX = PixelsPerDot * (position + 1) + PixelsPerDot / 2.0;
            double expectedY = PixelsPerDot * (row + 1) + PixelsPerDot / 2.0;
            if (hypot(centerX - expectedX, centerY - expectedY) > PixelsPerDot / 2.0)
                continue;
            assigned[row][position] = true;
            ProjectivePoint observed;
            if (!normalizedToImage.map(ProjectivePoint(centerX / width, centerY / height), observed))
                return false;
            SimplePoint2D modelPoint = model->dotLocation(position, row);
            modelPoints.push_back(ProjectivePoint(modelPoint.x, modelPoint.y));
            imagePoints.push_back(observed);
        }

    // The accepted silhouette supplies localization; isolated payload circles
    // supply the missing interior correspondences without assuming bit values.
    feature->projectiveDotModelPoints = modelPoints;
    feature->projectiveDotImagePoints = imagePoints;
    feature->projectiveDotCorrespondenceCount = static_cast<int>(modelPoints.size());
    if (modelPoints.size() < 4)
        return false;
    ProjectiveTransform fitted;
    if (!ProjectiveTransform::fromCorrespondences(modelPoints, imagePoints, fitted))
        return false;
    double squaredError = 0.0;
    for (size_t index = 0; index < modelPoints.size(); index++)
    {
        ProjectivePoint projected;
        if (!fitted.map(modelPoints[index], projected))
            return false;
        double dx = projected.x - imagePoints[index].x;
        double dy = projected.y - imagePoints[index].y;
        squaredError += dx * dx + dy * dy;
    }
    feature->projectiveDotRmsError = sqrt(squaredError / modelPoints.size());
    feature->projectiveDotRefined = std::isfinite(feature->projectiveDotRmsError) &&
        feature->projectiveOutlineError(fitted,
                                        feature->projectiveRefinedOutlineRmsError,
                                        feature->projectiveRefinedOutlineMaxError);
    if (feature->projectiveDotRefined)
        feature->projectiveRefinedOutlineNearFraction =
            feature->projectiveOutlineNearFraction(fitted, 0.04);
    // Preserve the historical model-check rule: more than 80% of the accepted
    // outline must remain within 0.04 model units after dot-derived refinement.
    feature->projectiveDotRefined = feature->projectiveDotRefined &&
        feature->projectiveRefinedOutlineNearFraction > 0.8;
    if (feature->projectiveDotRefined)
        feature->projectiveMat = fitted;
    return feature->projectiveDotRefined;
}

struct CarrierTemplateFit
{
    CarrierTemplateFit()
        : valid(false), boundary(false), ambiguous(false), amount(0.0),
          bestLoss(std::numeric_limits<int>::max()),
          alternativeLoss(std::numeric_limits<int>::max()),
          structuralMismatchPixels(0), structuralSupportPixels(0),
          structuralBestMismatchPixels(std::numeric_limits<int>::max()),
          structuralBestAmount(0.0), distinctPayloads(0) { }
    bool valid;
    bool boundary;
    bool ambiguous;
    double amount;
    int bestLoss;
    int alternativeLoss;
    std::string alternativePayload;
    int structuralMismatchPixels;
    int structuralSupportPixels;
    int structuralBestMismatchPixels;
    double structuralBestAmount;
    std::string structuralBestPayload;
    int distinctPayloads;
    std::string payload;
    ProjectiveTransform normalizedToImage;
};

struct CarrierTemplateEvidence
{
    CarrierTemplateEvidence()
        : structuralMismatchPixels(0), structuralSupportPixels(0) { }
    int structuralMismatchPixels;
    int structuralSupportPixels;
};

static bool oppositePixel(int value, int background)
{
    int contrast = abs(background - value);
    bool backgroundIsWhite = background >= 128;
    return contrast > RadianceDistMatch &&
        (backgroundIsWhite ? value < 160 : value > 96);
}

static int carrierTemplateLoss(RawImageGray8 *patch, std::string &payload,
                               CarrierTemplateEvidence *evidence = nullptr)
{
    const int radius = PixelsPerDot / 2;
    int backgroundTotal = 0;
    int backgroundCount = 0;
    for (int dy = -radius; dy <= radius; dy++)
        for (int dx = -radius; dx <= radius; dx++)
            if (dx * dx + dy * dy < radius * radius)
            {
                backgroundTotal += patch->getPixel(radius + dx, radius + dy);
                backgroundCount++;
            }
    if (backgroundCount == 0)
        return std::numeric_limits<int>::max();
    int background = backgroundTotal / backgroundCount;
    int width = patch->getSizeX();
    int height = patch->getSizeY();
    std::vector<unsigned char> observed(width * height, 0);
    std::vector<unsigned char> carrierMismatch(width * height, 0);
    std::vector<unsigned char> payloadSupport(width * height, 0);
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            observed[y * width + x] = oppositePixel(patch->getPixel(x, y), background);

    QyooModel *model = QyooModel::getQyooModel();
    SimplePoint2D lower, upper;
    model->dotBounds(lower, upper, true);
    double spanX = upper.x - lower.x;
    double spanY = upper.y - lower.y;
    int loss = 0;
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            double modelX = lower.x + spanX * x / width;
            double modelY = lower.y + spanY * y / height;
            bool insideCircle = (modelX - 0.5) * (modelX - 0.5) +
                                (modelY - 0.5) * (modelY - 0.5) <= 0.25;
            bool insideSquare = modelX <= 0.5 && modelY <= 0.5;
            bool predictedOpposite = !(insideCircle || insideSquare);
            bool mismatch = predictedOpposite != static_cast<bool>(observed[y * width + x]);
            carrierMismatch[y * width + x] = mismatch;
            if (mismatch)
                loss++;
        }

    payload.assign(QYOOSIZE * QYOOSIZE, '0');
    double dotRadius = model->dotRadius();
    for (int row = 0; row < model->numRows(); row++)
        for (int position = 0; position < model->numPos(); position++)
        {
            SimplePoint2D center = model->dotLocation(position, row);
            int zeroLoss = 0;
            int oneLoss = 0;
            for (int y = 0; y < height; y++)
                for (int x = 0; x < width; x++)
                {
                    double modelX = lower.x + spanX * x / width;
                    double modelY = lower.y + spanY * y / height;
                    double dx = modelX - center.x;
                    double dy = modelY - center.y;
                    if (dx * dx + dy * dy >= dotRadius * dotRadius)
                        continue;
                    payloadSupport[y * width + x] = 1;
                    bool value = observed[y * width + x];
                    zeroLoss += value;
                    oneLoss += !value;
                }
            if (oneLoss < zeroLoss)
            {
                loss += oneLoss - zeroLoss;
                int bitIndex = (QYOOSIZE - row - 1) * QYOOSIZE +
                               (QYOOSIZE - position - 1);
                payload[bitIndex] = '1';
            }
        }
    if (evidence)
        for (int index = 0; index < width * height; index++)
            if (!payloadSupport[index])
            {
                evidence->structuralSupportPixels++;
                if (carrierMismatch[index])
                    evidence->structuralMismatchPixels++;
            }
    return loss;
}

static CarrierTemplateFit fitCarrierTemplate(gdImagePtr input,
                                             const ProjectiveTransform &inputScale,
                                             const ProjectiveTransform &outline,
                                             const ProjectiveTransform &dotTranslation,
                                             const ProjectiveTransform &dotScale,
                                             int patchWidth, int patchHeight,
                                             int searchSteps,
                                             double sourceOffsetX = 0.0,
                                             double sourceOffsetY = 0.0)
{
    CarrierTemplateFit result;
    ProjectiveTransform sourceOffset(1.0, 0.0, sourceOffsetX,
                                     0.0, 1.0, sourceOffsetY,
                                     0.0, 0.0, 1.0);
    std::map<std::string, int> payloadLosses;
    int bestIndex = -1;
    for (int index = 0; index < searchSteps; index++)
    {
        double fraction = static_cast<double>(index) / (searchSteps - 1);
        double amount = MinimumCarrierAmbiguityAmount +
            fraction * (MaximumCarrierAmbiguityAmount - MinimumCarrierAmbiguityAmount);
        ProjectiveTransform normalizedToImage = sourceOffset * inputScale * outline *
            QyooModel::carrierAmbiguityTransform(amount) * dotTranslation * dotScale;
        RawImageGray8 trial(patchWidth, patchHeight);
        if (!trial.copyFromGDImageProjective(input, normalizedToImage))
            continue;
        trial.runContrast();
        std::string payload;
        CarrierTemplateEvidence evidence;
        int loss = carrierTemplateLoss(&trial, payload, &evidence);
        if (evidence.structuralMismatchPixels < result.structuralBestMismatchPixels ||
            (evidence.structuralMismatchPixels == result.structuralBestMismatchPixels &&
             fabs(amount) < fabs(result.structuralBestAmount)))
        {
            result.structuralBestMismatchPixels = evidence.structuralMismatchPixels;
            result.structuralBestAmount = amount;
            result.structuralBestPayload = payload;
        }
        auto found = payloadLosses.find(payload);
        if (found == payloadLosses.end() || loss < found->second)
            payloadLosses[payload] = loss;
        if (loss < result.bestLoss ||
            (loss == result.bestLoss && fabs(amount) < fabs(result.amount)))
        {
            result.bestLoss = loss;
            result.amount = amount;
            result.payload = payload;
            result.normalizedToImage = normalizedToImage;
            result.structuralMismatchPixels = evidence.structuralMismatchPixels;
            result.structuralSupportPixels = evidence.structuralSupportPixels;
            bestIndex = index;
        }
    }
    if (bestIndex < 0)
        return result;
    result.distinctPayloads = static_cast<int>(payloadLosses.size());
    for (const auto &entry : payloadLosses)
        if (entry.first != result.payload && entry.second < result.alternativeLoss)
        {
            result.alternativeLoss = entry.second;
            result.alternativePayload = entry.first;
        }
    result.boundary = bestIndex == 0 || bestIndex == searchSteps - 1;
    result.ambiguous = result.alternativeLoss != std::numeric_limits<int>::max() &&
        result.alternativeLoss - result.bestLoss < MinimumCarrierTemplatePayloadGap;
    result.valid = !result.boundary && !result.ambiguous;
    return result;
}

// FeatureDotsProcessor constructor
// Initializes the dot processor with an image, a feature processor, and a feature.
FeatureDotsProcessor::FeatureDotsProcessor(gdImagePtr inImage, FeatureProcessor *inFeatProc,
                                           Feature *inFeat, NormalizationMode inNormalization,
                                           const std::string &inResultLabel,
                                           ProjectiveFallbackPolicy inFallbackPolicy,
                                           bool inEmitResult, bool inWriteOutputImage)
{
    init(inImage, inFeatProc, inFeat, inNormalization, inResultLabel,
         inFallbackPolicy, inEmitResult, inWriteOutputImage);
}

// Initialize the dot processor
void FeatureDotsProcessor::init(gdImagePtr inImage, FeatureProcessor *inFeatProc, Feature *inFeat,
                                NormalizationMode inNormalization, const std::string &inResultLabel,
                                ProjectiveFallbackPolicy inFallbackPolicy, bool inEmitResult,
                                bool inWriteOutputImage)
{
    grayImg = nullptr;
    gaussImg = nullptr;
    gradImg = nullptr;
    featProc = inFeatProc;
    feat = inFeat;
    normalization = inNormalization;
    resultLabel = inResultLabel;
    fallbackPolicy = inFallbackPolicy;
    emitResult = inEmitResult;
    writeOutputImage = inWriteOutputImage;
    normalizationAvailable = false;
	normalizedPatchToInputValid = false;
    affinePilotToInputValid = false;
	carrierTemplateAttempted = false;
	carrierTemplateAvailable = false;
	carrierTemplateAmbiguous = false;
	carrierTemplateBoundaryRejected = false;
	carrierTemplateSamplerDisagreed = false;
	carrierTemplateAmbiguityAmount = 0.0;
	carrierTemplateBestLoss = -1;
	carrierTemplateAlternativeLoss = -1;
	carrierTemplateAlternativePayload.clear();
	carrierTemplateStructuralMismatchPixels = 0;
	carrierTemplateStructuralSupportPixels = 0;
	carrierTemplateStructuralBestMismatchPixels = 0;
	carrierTemplateStructuralBestAmount = 0.0;
	carrierTemplateStructuralBestPayload.clear();
	carrierTemplateStructuralConsistencyRejected = false;
	carrierTemplateStructuralAlternativeRejected = false;
	carrierTemplatePhaseAuditAttempted = false;
	carrierTemplatePhaseValidFits = 0;
	carrierTemplatePhaseDisagreements = 0;
	carrierTemplateRasterConfirmationRequired = false;
	carrierTemplateRasterConfirmationAttempted = false;
	carrierTemplateRasterConfirmationPassed = false;
	carrierTemplateDistinctPayloads = 0;
	carrierTemplateSearchSteps = 0;
	carrierTemplatePayload.clear();
	backgroundAverage = -1;
	for (int row = 0; row < QYOOSIZE; row++)
		for (int position = 0; position < QYOOSIZE; position++)
		{
			sampledBits[row][position] = -1;
			sampleValidCount[row][position] = 0;
			sampleMatchCount[row][position] = 0;
			sampleCenterGray[row][position] = -1;
			sampleMatchRatio[row][position] = 0.0;
			sampleMeanGray[row][position] = 0.0;
			sampleMedianGray[row][position] = 0.0;
			sampleDecisionMargin[row][position] = 0.0;
		}
	if (normalization == NormalizationAffine)
		feat->affineNormalizationAttempted = true;
	else if (normalization == NormalizationProjective ||
	         normalization == NormalizationCarrierTemplate)
		feat->projectiveNormalizationAttempted = true;

    QyooModel *qyooModel = QyooModel::getQyooModel();
    float imgWidth = gdImageSX(inImage);
    float imgHeight = gdImageSY(inImage);

    // Determine the size of the image to render to
    int sizeX = PixelsPerDot * (qyooModel->numRows() + 2);
    int sizeY = PixelsPerDot * (qyooModel->numPos() + 2);

    // Apply transformations to convert normalized dot-crop coordinates to the source image.
    float scaleX = imgWidth / feat->imgSizeX;
    float scaleY = imgHeight / feat->imgSizeY;

    // Add a margin of space around the dots
    SimplePoint2D ll, ur;
    qyooModel->dotBounds(ll, ur, true);

    // Convert the image to grayscale and apply contrast
    grayImg = new RawImageGray8(sizeX, sizeY);
    if (normalization == NormalizationProjective ||
        normalization == NormalizationCarrierTemplate)
    {
        if (!feat->projectiveValid)
            return;
        ProjectiveTransform inputScale(scaleX, 0.0, 0.0,
                                       0.0, scaleY, 0.0,
                                       0.0, 0.0, 1.0);
        ProjectiveTransform dotTranslation(1.0, 0.0, ll.x,
                                           0.0, 1.0, ll.y,
                                           0.0, 0.0, 1.0);
        ProjectiveTransform dotScale(ur.x - ll.x, 0.0, 0.0,
                                    0.0, ur.y - ll.y, 0.0,
                                    0.0, 0.0, 1.0);
        ProjectiveTransform affineFeature(
            feat->mat(0, 0), feat->mat(0, 1), feat->mat(0, 2),
            feat->mat(1, 0), feat->mat(1, 1), feat->mat(1, 2),
            feat->mat(2, 0), feat->mat(2, 1), feat->mat(2, 2));
        ProjectiveTransform affineNormalizedToImage = inputScale * affineFeature * dotTranslation * dotScale;
        affinePilotToInput = affineNormalizedToImage;
        affinePilotToInputValid = true;
        RawImageGray8 affinePilot(sizeX, sizeY);
        if (!affinePilot.copyFromGDImageProjective(inImage, affineNormalizedToImage))
            return;
        affinePilot.runContrast();
        if (normalization == NormalizationCarrierTemplate)
        {
            carrierTemplateAttempted = true;
            if (!feat->carrierProjectiveValid && !feat->estimateCarrierProjectiveClass())
            {
                logVerbose(featProc ? featProc->logger : nullptr,
                           "Carrier line/conic projective class unavailable; payload rejected.");
                return;
            }
            CarrierTemplateFit fit = fitCarrierTemplate(
                inImage, inputScale, feat->carrierProjectiveMat, dotTranslation, dotScale,
                sizeX * 2, sizeY * 2, CarrierAmbiguitySearchSteps);
            carrierTemplateSearchSteps = CarrierAmbiguitySearchSteps;
            if (fit.ambiguous && !fit.boundary)
            {
                fit = fitCarrierTemplate(
                    inImage, inputScale, feat->carrierProjectiveMat,
                    dotTranslation, dotScale, sizeX * 2, sizeY * 2,
                    2 * CarrierAmbiguitySearchSteps - 1);
                carrierTemplateSearchSteps = 2 * CarrierAmbiguitySearchSteps - 1;
            }
            carrierTemplateAmbiguityAmount = fit.amount;
            carrierTemplateBestLoss = fit.bestLoss == std::numeric_limits<int>::max()
                ? -1 : fit.bestLoss;
            carrierTemplateAlternativeLoss =
                fit.alternativeLoss == std::numeric_limits<int>::max()
                ? -1 : fit.alternativeLoss;
            carrierTemplateAlternativePayload = fit.alternativePayload;
            carrierTemplateStructuralMismatchPixels = fit.structuralMismatchPixels;
            carrierTemplateStructuralSupportPixels = fit.structuralSupportPixels;
            carrierTemplateStructuralBestMismatchPixels =
                fit.structuralBestMismatchPixels == std::numeric_limits<int>::max()
                ? 0 : fit.structuralBestMismatchPixels;
            carrierTemplateStructuralBestAmount = fit.structuralBestAmount;
            carrierTemplateStructuralBestPayload = fit.structuralBestPayload;
            carrierTemplateDistinctPayloads = fit.distinctPayloads;
            carrierTemplatePayload = fit.payload;
            carrierTemplateAmbiguous = fit.ambiguous;
            carrierTemplateBoundaryRejected = fit.boundary;
            if (!fit.valid)
            {
                logVerbose(featProc ? featProc->logger : nullptr, fit.boundary
                    ? "Carrier-template optimum reached search boundary; payload rejected."
                    : "Carrier-template payload alternatives are ambiguous; payload rejected.");
                return;
            }
            carrierTemplateStructuralConsistencyRejected =
                fit.structuralSupportPixels <= 0 ||
                fit.structuralMismatchPixels * 100 >=
                    fit.structuralSupportPixels * MaximumCarrierStructuralMismatchPercent;
            int structuralImprovement = fit.structuralMismatchPixels -
                                        fit.structuralBestMismatchPixels;
            carrierTemplateStructuralAlternativeRejected =
                fit.structuralBestPayload != fit.payload &&
                structuralImprovement >= MinimumStructuralAlternativeImprovementPixels &&
                structuralImprovement * 100 >= fit.structuralMismatchPixels *
                    MinimumStructuralAlternativeImprovementPercent;
            if (carrierTemplateStructuralConsistencyRejected ||
                carrierTemplateStructuralAlternativeRejected)
            {
                logVerbose(featProc ? featProc->logger : nullptr,
                           carrierTemplateStructuralConsistencyRejected
                    ? "Carrier-template structure disagrees with the Qyoo carrier; payload rejected."
                    : "A projective-class alternative has materially stronger carrier evidence; payload rejected.");
                return;
            }
            if (fit.valid && fit.alternativeLoss != std::numeric_limits<int>::max() &&
                fit.alternativeLoss - fit.bestLoss < CarrierTemplatePhaseAuditGapPixels)
            {
                carrierTemplatePhaseAuditAttempted = true;
                const double offsets[][2] = {
                    {-0.5, 0.0}, {0.5, 0.0}, {0.0, -0.5}, {0.0, 0.5}
                };
                for (const auto &offset : offsets)
                {
                    CarrierTemplateFit phaseFit = fitCarrierTemplate(
                        inImage, inputScale, feat->carrierProjectiveMat,
                        dotTranslation, dotScale, sizeX * 2, sizeY * 2,
                        carrierTemplateSearchSteps, offset[0], offset[1]);
                    if (!phaseFit.valid)
                        continue;
                    carrierTemplatePhaseValidFits++;
                    if (phaseFit.payload != fit.payload)
                        carrierTemplatePhaseDisagreements++;
                }
            }
            carrierTemplateRasterConfirmationRequired =
                carrierTemplatePhaseDisagreements >= RasterConfirmationPhaseDisagreements;
            if (!grayImg->copyFromGDImageProjective(inImage, fit.normalizedToImage))
                return;
            normalizedPatchToInput = fit.normalizedToImage;
            normalizedPatchToInputValid = true;
            carrierTemplateAvailable = true;
            logVerbose(featProc ? featProc->logger : nullptr,
                       "Carrier-template fit: amount " + std::to_string(fit.amount) +
                       ", loss " + std::to_string(fit.bestLoss) +
                       ", alternative " +
                       (fit.alternativeLoss == std::numeric_limits<int>::max()
                           ? std::string("none") : std::to_string(fit.alternativeLoss)) +
                       ", payloads " + std::to_string(fit.distinctPayloads));
        }
        else if (refineProjectiveFromVisibleDots(&affinePilot, affineNormalizedToImage, feat))
        {
            logVerbose(featProc ? featProc->logger : nullptr, "Projective dot refinement: " +
                       std::to_string(feat->projectiveDotCorrespondenceCount) +
                       " centers, RMS " + std::to_string(feat->projectiveDotRmsError));
            logVerbose(featProc ? featProc->logger : nullptr, "Projective dot matrix: [" +
                       std::to_string(feat->projectiveMat.at(0, 0)) + "," +
                       std::to_string(feat->projectiveMat.at(0, 1)) + "," +
                       std::to_string(feat->projectiveMat.at(0, 2)) + ";" +
                       std::to_string(feat->projectiveMat.at(1, 0)) + "," +
                       std::to_string(feat->projectiveMat.at(1, 1)) + "," +
                       std::to_string(feat->projectiveMat.at(1, 2)) + ";" +
                       std::to_string(feat->projectiveMat.at(2, 0)) + "," +
                       std::to_string(feat->projectiveMat.at(2, 1)) + "," +
                       std::to_string(feat->projectiveMat.at(2, 2)) + "]");
            logVerbose(featProc ? featProc->logger : nullptr,
                       "Projective refined-outline fit: RMS " +
                       std::to_string(feat->projectiveRefinedOutlineRmsError) +
                       ", max " + std::to_string(feat->projectiveRefinedOutlineMaxError) +
                       ", near fraction " +
                       std::to_string(feat->projectiveRefinedOutlineNearFraction));
        }
        else
        {
            if (feat->projectiveDotCorrespondenceCount >= 4)
            {
                logVerbose(featProc ? featProc->logger : nullptr,
                           "Projective rejected dot refinement: " +
                           std::to_string(feat->projectiveDotCorrespondenceCount) +
                           " centers, dot RMS " +
                           std::to_string(feat->projectiveDotRmsError) +
                           ", outline RMS " +
                           std::to_string(feat->projectiveRefinedOutlineRmsError) +
                           ", outline max " +
                           std::to_string(feat->projectiveRefinedOutlineMaxError) +
                           ", near fraction " +
                           std::to_string(feat->projectiveRefinedOutlineNearFraction));
                logVerbose(featProc ? featProc->logger : nullptr,
                           "Projective dot refinement rejected by accepted-outline validation; payload not sampled.");
                feat->projectiveValid = false;
                return;
            }
            bool allowFallback = fallbackPolicy == LegacyAffineFallback ||
                (fallbackPolicy == QualifiedAffineFallback &&
                 feat->affineFallbackGeometryQualified(MaximumQualifiedAffineAngleDeviationDegrees));
            if (!allowFallback)
            {
                feat->projectiveAffineFallbackRejected = true;
                logVerbose(featProc ? featProc->logger : nullptr,
                           "Projective dot refinement unavailable; affine fallback rejected by policy.");
                feat->projectiveValid = false;
                return;
            }
            logVerbose(featProc ? featProc->logger : nullptr,
                       "Projective dot refinement unavailable; using affine fallback.");
			feat->projectiveAffineFallbackUsed = true;
        }
        if (normalization == NormalizationProjective)
        {
            ProjectiveTransform normalizedToImage = feat->projectiveDotRefined
                ? inputScale * feat->projectiveMat * dotTranslation * dotScale
                : affineNormalizedToImage;
            if (!grayImg->copyFromGDImageProjective(inImage, normalizedToImage))
                return;
            normalizedPatchToInput = normalizedToImage;
            normalizedPatchToInputValid = true;
        }
    }
    else
    {
        QyooMatrix inputScale(scaleX, 0.0, 0.0, 0.0, scaleY, 0.0, 0.0, 0.0, 1.0);
        QyooMatrix forMat = inputScale * feat->mat;
        QyooMatrix dotTranslation(1.0, 0.0, ll.x, 0.0, 1.0, ll.y, 0.0, 0.0, 1.0);
        QyooMatrix dotScale(ur.x - ll.x, 0.0, 0.0,
                            0.0, ur.y - ll.y, 0.0,
                            0.0, 0.0, 1.0);
        forMat = forMat * dotTranslation * dotScale;
        QyooMatrix inverseForMat = forMat;
        inverseForMat.inverse();
        grayImg->copyFromGDImage(inImage, inverseForMat);
        normalizedPatchToInput = ProjectiveTransform(
            forMat(0, 0), forMat(0, 1), forMat(0, 2),
            forMat(1, 0), forMat(1, 1), forMat(1, 2),
            forMat(2, 0), forMat(2, 1), forMat(2, 2));
        normalizedPatchToInputValid = true;
    }
    grayImg->runContrast();
    normalizationAvailable = true;
	if (normalization == NormalizationAffine)
		feat->affineNormalizationAvailable = true;
	else if (normalization == NormalizationProjective ||
	         normalization == NormalizationCarrierTemplate)
		feat->projectiveNormalizationAvailable = true;
}

// Destructor for the dot processor
FeatureDotsProcessor::~FeatureDotsProcessor()
{
    delete grayImg;
    delete gaussImg;
    delete gradImg;
    feat = nullptr;
}

// Calculate the average pixel value in a region
static int calcAvgPixel(RawImageGray8 *img, int px, int py, ConvolutionFilterInt *radFilter)
{
    std::vector<int> results(radFilter->getSize() * radFilter->getSize());
    radFilter->processPixel(img, px, py, &results[0]);

    int val = 0;
    int validSamples = 0;
    for (int result : results) {
        if (result < 0)
            continue;
        val += result;
        validSamples++;
    }

    return validSamples > 0 ? val / validSamples : 0;
}

// Constants used for dot detection
// Check if an area contains a dot by comparing the radiance of pixels
struct DotSampleMeasurement
{
	int validCount;
	int matchCount;
	int centerGray;
	double ratio;
	double meanGray;
	double medianGray;
	double decisionMargin;
};

static bool isAdot(RawImageGray8 *img,int px,int py,int pixelsInDot,
	ConvolutionFilterInt *radFilter,int backColor, DotSampleMeasurement *measurement = nullptr)
{
	std::vector<int> results(radFilter->getSize()*radFilter->getSize());
	radFilter->processPixel(img,px,py,&results[0]);

	// Decide if the background color is "blackish" or "whiteish"
	bool isWhite = (backColor >= 128);

	// Run through and look for matching pixels
	int numMatch = 0;
	std::vector<int> validValues;
	for (unsigned int ii=0;ii<radFilter->getSize()*radFilter->getSize();ii++)
	{
		int thisColor = results[ii];
		if (thisColor >= 0)
		{
			validValues.push_back(thisColor);
			// Distance from this pixel to the grey value we're after
			int dist = backColor - thisColor;  if (dist < 0) dist *= -1;

			// The radiance needs to be far enough away and it needs to be
			//  on the opposite side of 128
			if (dist > RadianceDistMatch && ((isWhite && thisColor < 128+32) ||(!isWhite && thisColor > 128-32)))
				numMatch++;
		}
	}

	float ratio = (float)numMatch / (float)radFilter->getFact();
	if (measurement)
	{
		measurement->validCount = static_cast<int>(validValues.size());
		measurement->matchCount = numMatch;
		measurement->centerGray = img->getPixel(px, py);
		measurement->ratio = ratio;
		measurement->decisionMargin = ratio - PassRatio;
		double total = 0.0;
		for (int value : validValues) total += value;
		measurement->meanGray = validValues.empty() ? 0.0 : total / validValues.size();
		std::sort(validValues.begin(), validValues.end());
		if (validValues.empty()) measurement->medianGray = 0.0;
		else if (validValues.size() % 2 == 1)
			measurement->medianGray = validValues[validValues.size() / 2];
		else
			measurement->medianGray =
				(validValues[validValues.size() / 2 - 1] +
				 validValues[validValues.size() / 2]) / 2.0;
	}

	return (ratio >= PassRatio);
}

// Convert decimal to binary string representation
static std::string dec2bin(int intDec)
{
    std::string strBin;
    while (intDec)
    {
        std::stringstream sstream;
        sstream << (intDec % 2) << strBin;
        strBin = sstream.str();
        intDec /= 2;
    }

    // Pad with extra zeroes to fit six bits
    while (strBin.size() < 6)
        strBin = "0" + strBin;

    return strBin;
}

// Detect dots in a grayscale image and mark their locations
void FeatureDotsProcessor::findDotsGray() {
    if (!normalizationAvailable)
    {
        logVerbose(featProc ? featProc->logger : nullptr,
                   (resultLabel.empty() ? std::string("Projective") : resultLabel) +
                   " normalization unavailable; payload not sampled.");
        return;
    }
    QyooModel *qyooModel = QyooModel::getQyooModel();
    ConvolutionFilterInt *radFilter = MakeRadiusFilter(PixelsPerDot, PixelsPerDot / 2);

    int avgPixel = calcAvgPixel(grayImg, PixelsPerDot / 2, PixelsPerDot / 2, radFilter);
    backgroundAverage = avgPixel;

    int numRow = qyooModel->numRows();
    int numPos = qyooModel->numPos();
    feat->dotBinStr.clear();
    feat->dotDecStr.clear();
    qyooBits = "";  // Start fresh with qyooBits

    gdImagePtr outImg = nullptr;
    int colorRed = 0;
    int colorGreen = 0;
    if (writeOutputImage)
    {
        // Create the CLI's diagnostic image. Production library calls skip it.
        outImg = gdImageCreateTrueColor(grayImg->getSizeX(), grayImg->getSizeY());
        gdImagePtr grayVisualization = grayImg->makeGDImage();
        for (int x = 0; x < grayImg->getSizeX(); x++) {
            for (int y = 0; y < grayImg->getSizeY(); y++) {
                int grayValue = gdImageGetPixel(grayVisualization, x, y);
                int rgbColor = gdImageColorAllocate(outImg, grayValue, grayValue, grayValue);
                gdImageSetPixel(outImg, x, y, rgbColor);
            }
        }
        gdImageDestroy(grayVisualization);
        colorRed = gdImageColorAllocate(outImg, 255, 0, 0);
        colorGreen = gdImageColorAllocate(outImg, 0, 255, 0);
    }

    for (int row = 0; row < numRow; row++) {  // We process from row 0 to numRow
        int resChar = 0;
        int rowPix = PixelsPerDot * (row + 1) + PixelsPerDot / 2;

        for (unsigned int pos = 0; pos < numPos; pos++) {
            int posPix = PixelsPerDot * (pos + 1) + PixelsPerDot / 2;

            DotSampleMeasurement measurement;
            bool decoded = isAdot(grayImg, posPix, rowPix, PixelsPerDot,
                                  radFilter, avgPixel, &measurement);
            sampleValidCount[row][pos] = measurement.validCount;
            sampleMatchCount[row][pos] = measurement.matchCount;
            sampleCenterGray[row][pos] = measurement.centerGray;
            sampleMatchRatio[row][pos] = measurement.ratio;
            sampleMeanGray[row][pos] = measurement.meanGray;
            sampleMedianGray[row][pos] = measurement.medianGray;
            sampleDecisionMargin[row][pos] = measurement.decisionMargin;

            if (decoded) {
                resChar |= 1 << pos;
                sampledBits[row][pos] = 1;

                // Draw a green circle around the detected dot
                if (outImg)
                    gdImageArc(outImg, posPix, rowPix, PixelsPerDot, PixelsPerDot,
                               0, 360, colorGreen);
            } else {
                sampledBits[row][pos] = 0;
                // If the dot represents a 0, draw a red X
                if (outImg)
                {
                    gdImageLine(outImg, posPix - 5, rowPix - 5,
                                posPix + 5, rowPix + 5, colorRed); // Draw slash left
                    gdImageLine(outImg, posPix - 5, rowPix + 5,
                                posPix + 5, rowPix - 5, colorRed); // Draw slash right
                }
            }
        }

        // rows are read in reverse, and need to be reversed again
        unsigned char theChar;
        std::string currentRowBits = dec2bin(resChar);  // Get binary string representation of resChar
        qyooBits = currentRowBits + qyooBits;  // Prepend binary string (reversing row order)

        if (numRow - row - 1 >= 0 && numRow - row - 1 < QYOOSIZE) {
            qyooRows[numRow - row - 1] = resChar;  // Store in reverse row order
        } else {
            std::cerr << "Error: Invalid access to qyooRows at index " << (numRow - row - 1) << std::endl;
        }

        qyooModel->bitsToChar(resChar, theChar);
        feat->dotBits.push_back(resChar);

        // Debugging output for each row

        logVerbose(featProc ? featProc->logger : nullptr,
                   "Row " + std::to_string(row) + ": resChar (binary) = " + currentRowBits);
        logVerbose(featProc ? featProc->logger : nullptr,
                   "Current qyooBits = " + qyooBits);
    }
    if (normalization == NormalizationCarrierTemplate &&
        qyooBits != carrierTemplatePayload)
    {
        carrierTemplateSamplerDisagreed = true;
        logVerbose(featProc ? featProc->logger : nullptr,
                   "Carrier-template fitted payload and deterministic sampler disagree; payload rejected.");
        qyooBits.clear();
        feat->dotBits.clear();
        if (outImg)
            gdImageDestroy(outImg);
        delete radFilter;
        return;
    }
    feat->dotBinStr = qyooBits;

    // Convert the full binary string (qyooBits) to decimal, but first ensure it's within the range of an unsigned long long
    if (qyooBits.size() > 64) {
        std::cerr << "Error: qyooBits exceeds 64 bits, cannot convert to unsigned long long." << std::endl;
    } else {
        std::string prefix = resultLabel.empty() ? "" : resultLabel + " ";
        if (emitResult)
            std::cout << prefix << "Binary = " << qyooBits << std::endl;

        feat->dotDecStr = std::to_string(std::stoull(qyooBits, nullptr, 2));  // Convert binary string to decimal
        if (emitResult)
            std::cout << prefix << "Qyoo value = " << feat->dotDecStr << std::endl;
		if (normalization == NormalizationAffine)
			feat->affinePayloadExtracted = true;
		else if (normalization == NormalizationProjective ||
		         normalization == NormalizationCarrierTemplate)
			feat->projectivePayloadExtracted = true;
    }

    // Save the image with the dots circled and x notated to see where pattern is detected
    if (writeOutputImage)
    {
        std::string outputFilePrefix = resultLabel.empty() ? "" : resultLabel + "_";
        std::string outputFileName = "output/" + outputFilePrefix + feat->dotDecStr + ".png";
        FILE *outputFile = fopen(outputFileName.c_str(), "wb");
        if (outputFile) {
            gdImagePng(outImg, outputFile); // Save PNG image using gdImagePng
            fclose(outputFile);
        } else {
            std::cerr << "Error: Unable to open file for writing PNG image: " << outputFileName << std::endl;
        }
    }

    if (outImg)
        gdImageDestroy(outImg);

    delete radFilter;

}


// FeatureProcessor constructor: initialize with an image
FeatureProcessor::FeatureProcessor(gdImagePtr inImage, int sizeX, int sizeY,
                                   DetectorLogger *inLogger)
{
    gaussFilter = nullptr;
    grayImg = nullptr;
    gaussImg = nullptr;
    gradImg = nullptr;
    thetaImg = nullptr;
    featImg = nullptr;
    numFound = 0;
    logger = inLogger;
    grayImg = new RawImageGray8(sizeX, sizeY);
    grayImg->copyFromGDImage(inImage);
    grayImg->runContrast();
}

// Destructor for FeatureProcessor
FeatureProcessor::~FeatureProcessor()
{
    delete gaussFilter;
    delete grayImg;
    delete gaussImg;
    delete gradImg;
    delete thetaImg;
    delete featImg;
    for (auto *dot : featureDots)
        delete dot;
}

// Process the image to detect edges and gradients
void FeatureProcessor::processImage()
{
    gaussFilter = MakeGaussianFilter_1_4();
    gaussImg = new RawImageGray8(grayImg->getSizeX(), grayImg->getSizeY());

    // Apply Gaussian filter to reduce noise
    gaussFilter->processImage(grayImg, gaussImg);

    // Compute gradient and edge angle
    gradImg = new RawImageGray32(grayImg->getSizeX(), grayImg->getSizeY());
    thetaImg = new RawImageGray8(grayImg->getSizeX(), grayImg->getSizeY());
    CannyGradientAndTheta(gaussImg, gradImg, thetaImg);

    // Suppress non-maximum values to highlight edges
    CannyNonMaxSupress(gradImg, thetaImg, 60.0);
}

// Find valid Qyoo features
int FeatureProcessor::findQyoo()
{
    logVerbose(logger, "Starting Qyoo detection...");

    featImg = new RawImageGray32(grayImg->getSizeX(), grayImg->getSizeY());
    CannyFindFeatures(gradImg, thetaImg, 10.0, 60.0, feats, featImg, logger);

    logVerbose(logger, "Number of features detected: " + std::to_string(feats.size()) );

    // Iterate over the detected features and validate them
    numFound = 0;
    for (unsigned int ii = 0; ii < feats.size(); ii++)
    {
        Feature &feat = feats[ii];
        feat.logger = logger;

        feat.imgSizeX = grayImg->getSizeX();
        feat.imgSizeY = grayImg->getSizeY();

        feat.calcClosed(ClosedDist * ClosedDist);
        feat.decimate(DecimateDist * DecimateDist);
        feat.checkSizeAndPosition(grayImg->getSizeX(), grayImg->getSizeY());

        if (feat.valid)
        {
            feat.findCorner();
            feat.refineCornerAndFindAngles(10 * 10);
            feat.modelCheck(0.04 * 0.04, 0.8);
        }

        if (feat.valid)
        {
            logVerbose(logger, "Qyoo shape feature found!");
			feat.estimateProjectiveTransform();
			if (feat.projectiveValid)
			{
					logVerbose(logger, "Projective outline fit: " + std::to_string(feat.projectiveCorrespondenceCount) +
				           " points, RMS " + std::to_string(feat.projectiveRmsError) +
				           ", max " + std::to_string(feat.projectiveMaxError));
			}
			else
			{
					logVerbose(logger, "Projective outline fit unavailable.");
			}
            numFound++;
        }
    }

    logVerbose(logger, "Total Qyoo shapes detected: " + std::to_string(numFound) );
    return numFound;
}

// Detect dots in the valid Qyoo features
void FeatureProcessor::findDots(gdImagePtr inImage, NormalizationMode normalization,
                                ProjectiveFallbackPolicy fallbackPolicy, bool emitResults,
                                bool writeOutputImages)
{
    for (auto &feat : feats)
    {
        if (feat.valid)
        {
            if (normalization == NormalizationShadow)
            {
                auto *affineDots = new FeatureDotsProcessor(inImage, this, &feat,
                                                            NormalizationAffine, "", fallbackPolicy,
                                                            emitResults, writeOutputImages);
                affineDots->findDotsGray();
                featureDots.push_back(affineDots);
                auto *projectiveDots = new FeatureDotsProcessor(inImage, this, &feat,
                                                                NormalizationProjective, "Projective", fallbackPolicy,
                                                                emitResults, writeOutputImages);
                projectiveDots->findDotsGray();
                featureDots.push_back(projectiveDots);
            }
            else
            {
                auto *featDots = new FeatureDotsProcessor(inImage, this, &feat, normalization,
                                                          "", fallbackPolicy, emitResults,
                                                          writeOutputImages);
                featDots->findDotsGray();
                featureDots.push_back(featDots);
            }
        }
    }
}

static const char *normalizationModeCode(NormalizationMode normalization)
{
    switch (normalization)
    {
        case NormalizationAffine: return "affine";
        case NormalizationProjective: return "projective";
        case NormalizationCarrierTemplate: return "carrier-template";
        case NormalizationShadow: return "shadow";
    }
    return "unknown";
}

static std::string visualArtifactStem(size_t featureIndex,
                                      const FeatureDotsProcessor *dots)
{
    std::ostringstream output;
    output << "feature_" << std::setw(4) << std::setfill('0') << featureIndex
           << '_' << normalizationModeCode(dots->normalization);
    return output.str();
}

static bool writeGrayPng(RawImageGray8 *image, const std::string &path)
{
    if (!image)
        return false;
    gdImagePtr pngImage = image->makeGDImage();
    if (!pngImage)
        return false;
    FILE *file = fopen(path.c_str(), "wb");
    if (!file)
    {
        gdImageDestroy(pngImage);
        return false;
    }
    gdImagePng(pngImage, file);
    fclose(file);
    gdImageDestroy(pngImage);
    return true;
}

bool FeatureProcessor::writeVisualDebugArtifacts(gdImagePtr inImage,
                                                 const std::string &outputDirectory) const
{
    bool success = true;
    std::string separator = outputDirectory.empty() || outputDirectory.back() == '/'
        ? "" : "/";
    for (const FeatureDotsProcessor *dots : featureDots)
    {
        size_t featureIndex = feats.size();
        for (size_t index = 0; index < feats.size(); index++)
            if (&feats[index] == dots->feat)
            {
                featureIndex = index;
                break;
            }
        if (featureIndex == feats.size())
        {
            success = false;
            continue;
        }
        std::string stem = visualArtifactStem(featureIndex, dots);
        if (dots->normalizationAvailable)
            success = writeGrayPng(dots->grayImg,
                                   outputDirectory + separator + stem + "_rectified.png") && success;
        if (dots->affinePilotToInputValid)
        {
            RawImageGray8 pilot(dots->grayImg->getSizeX(), dots->grayImg->getSizeY());
            if (!pilot.copyFromGDImageProjective(inImage, dots->affinePilotToInput))
                success = false;
            else
            {
                pilot.runContrast();
                success = writeGrayPng(&pilot,
                                       outputDirectory + separator + stem + "_pilot.png") && success;
            }
        }
    }
    return success;
}

static const char *fallbackPolicyCode(ProjectiveFallbackPolicy policy)
{
    switch (policy)
    {
        case LegacyAffineFallback: return "legacy-affine";
        case RejectUnsupportedProjective: return "reject";
        case QualifiedAffineFallback: return "qualified";
    }
    return "unknown";
}

static std::string jsonString(const std::string &value)
{
    std::ostringstream output;
    output << '"';
    for (unsigned char character : value)
    {
        switch (character)
        {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character < 0x20)
                {
                    output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                           << static_cast<int>(character) << std::dec;
                }
                else
                    output << character;
        }
    }
    output << '"';
    return output.str();
}

static const char *jsonBoolean(bool value)
{
    return value ? "true" : "false";
}

static const char *normalizationOutcomeCode(
    const Feature &feature, NormalizationMode normalization,
    const FeatureDotsProcessor *carrierTemplateDots)
{
    if (!feature.valid)
        return "not_attempted_candidate_rejected";
    if (normalization == NormalizationAffine)
    {
        if (feature.affinePayloadExtracted) return "affine_payload_extracted";
        if (feature.affineNormalizationAvailable) return "payload_extraction_failed";
        return feature.affineNormalizationAttempted
            ? "affine_normalization_unavailable" : "not_attempted";
    }
    if (normalization == NormalizationCarrierTemplate && carrierTemplateDots)
    {
        if (carrierTemplateDots->carrierTemplateStructuralConsistencyRejected)
            return "carrier_template_structural_consistency_rejected";
        if (carrierTemplateDots->carrierTemplateStructuralAlternativeRejected)
            return "carrier_template_structural_alternative_rejected";
        if (carrierTemplateDots->carrierTemplateRasterConfirmationAttempted &&
            !carrierTemplateDots->carrierTemplateRasterConfirmationPassed)
            return "carrier_template_raster_confirmation_rejected";
        if (carrierTemplateDots->carrierTemplateBoundaryRejected)
            return "carrier_template_search_boundary_rejected";
        if (carrierTemplateDots->carrierTemplateAmbiguous)
            return "carrier_template_ambiguous_rejected";
        if (carrierTemplateDots->carrierTemplateSamplerDisagreed)
            return "carrier_template_sampler_disagreement_rejected";
    }
    if (feature.projectivePayloadExtracted)
    {
        if (feature.projectiveDotRefined) return "projective_refined_payload_extracted";
        if (feature.projectiveAffineFallbackUsed) return "affine_fallback_payload_extracted";
        return "projective_payload_extracted";
    }
    if (feature.projectiveDotCorrespondenceCount >= 4 && !feature.projectiveDotRefined)
        return "projective_fit_rejected";
    if (feature.projectiveAffineFallbackRejected)
        return "insufficient_interior_correspondences_fallback_rejected";
    if (feature.projectiveNormalizationAvailable)
        return "payload_extraction_failed";
    return feature.projectiveNormalizationAttempted
        ? "projective_normalization_unavailable" : "not_attempted";
}

static void writeProjectiveMatrixJson(std::ostringstream &output,
                                      const ProjectiveTransform &matrix)
{
    output << '[';
    for (int row = 0; row < 3; row++)
    {
        if (row != 0) output << ',';
        output << '[';
        for (int column = 0; column < 3; column++)
        {
            if (column != 0) output << ',';
            output << matrix.at(row, column);
        }
        output << ']';
    }
    output << ']';
}

static void writeAffineMatrixJson(std::ostringstream &output,
                                  const QyooMatrix &matrix)
{
    output << '[';
    for (int row = 0; row < 3; row++)
    {
        if (row != 0) output << ',';
        output << '[';
        for (int column = 0; column < 3; column++)
        {
            if (column != 0) output << ',';
            output << matrix(row, column);
        }
        output << ']';
    }
    output << ']';
}

static void writeFeaturePointsJson(std::ostringstream &output,
                                   const std::list<Feature::Point> &points)
{
    output << '[';
    bool first = true;
    for (const Feature::Point &point : points)
    {
        if (!first) output << ',';
        first = false;
        output << '[' << point.x << ',' << point.y << ']';
    }
    output << ']';
}

static void writeProjectivePointsJson(std::ostringstream &output,
                                      const std::vector<ProjectivePoint> &points)
{
    output << '[';
    for (size_t index = 0; index < points.size(); index++)
    {
        if (index != 0) output << ',';
        output << '[' << points[index].x << ',' << points[index].y << ']';
    }
    output << ']';
}

std::string FeatureProcessor::diagnosticsJson(const std::string &imageId,
                                              NormalizationMode normalization,
                                              ProjectiveFallbackPolicy fallbackPolicy,
                                              bool includeVisualGeometry) const
{
    const FeatureRejectionReason reasons[] = {
        FeatureNotRejected,
        FeatureDegenerateBounds,
        FeatureAspectRatioRejected,
        FeatureTooSmall,
        FeatureTooLarge,
        FeatureCornerNotFound,
        FeatureCornerEdgesInsufficient,
        FeatureCornerAngleRejected,
        FeatureFarEdgeRejected,
        FeatureOutlineModelRejected
    };
    std::map<FeatureRejectionReason, int> reasonCounts;
    int sizePassed = 0;
    int cornerSelected = 0;
    int cornerGeometryPassed = 0;
    int outlinePassed = 0;
    int projectiveOutlineAvailable = 0;
    int affineAttempted = 0;
    int projectiveAttempted = 0;
    int affineFallback = 0;
    int affineFallbackRejected = 0;
    int normalizationAvailable = 0;
    int payloadExtracted = 0;
    for (const Feature &feature : feats)
    {
        reasonCounts[feature.rejectionReason]++;
        sizePassed += feature.sizeCheckPassed;
        cornerSelected += feature.cornerValid;
        cornerGeometryPassed += feature.cornerGeometryPassed;
        outlinePassed += feature.outlineCheckPassed;
        projectiveOutlineAvailable += feature.projectiveCorrespondenceCount > 0;
        affineAttempted += feature.affineNormalizationAttempted;
        projectiveAttempted += feature.projectiveNormalizationAttempted;
        affineFallback += feature.projectiveAffineFallbackUsed;
        affineFallbackRejected += feature.projectiveAffineFallbackRejected;
        normalizationAvailable += feature.affineNormalizationAvailable ||
                                  feature.projectiveNormalizationAvailable;
        payloadExtracted += feature.affinePayloadExtracted || feature.projectivePayloadExtracted;
    }

    std::ostringstream output;
    output << std::setprecision(12);
    output << "{\"schema\":\"org.qyoo.detector.rejection-diagnostics\",\"schema_version\":1";
    output << ",\"image_id\":" << jsonString(imageId);
    output << ",\"image_loaded\":true";
    output << ",\"image_size\":{\"width\":" << grayImg->getSizeX()
           << ",\"height\":" << grayImg->getSizeY() << "}";
    output << ",\"normalization_requested\":" << jsonString(normalizationModeCode(normalization));
    output << ",\"fallback_policy_requested\":" << jsonString(fallbackPolicyCode(fallbackPolicy));
    output << ",\"visual_debug_geometry_included\":" << jsonBoolean(includeVisualGeometry);
    output << ",\"stages\":{";
    output << "\"raw_feature_count\":" << feats.size();
    output << ",\"size_pass_count\":" << sizePassed;
    output << ",\"corner_selected_count\":" << cornerSelected;
    output << ",\"corner_geometry_pass_count\":" << cornerGeometryPassed;
    output << ",\"outline_validation_pass_count\":" << outlinePassed;
    output << ",\"accepted_candidate_count\":" << numFound;
    output << ",\"projective_outline_available_count\":" << projectiveOutlineAvailable;
    output << ",\"affine_normalization_attempted_count\":" << affineAttempted;
    output << ",\"projective_normalization_attempted_count\":" << projectiveAttempted;
    output << ",\"affine_fallback_count\":" << affineFallback;
    output << ",\"affine_fallback_rejected_count\":" << affineFallbackRejected;
    output << ",\"normalization_available_count\":" << normalizationAvailable;
    output << ",\"payload_extracted_count\":" << payloadExtracted << "}";
    output << ",\"rejection_reason_counts\":{";
    for (size_t index = 0; index < sizeof(reasons) / sizeof(reasons[0]); index++)
    {
        if (index != 0) output << ',';
        output << jsonString(featureRejectionReasonCode(reasons[index])) << ':'
               << reasonCounts[reasons[index]];
    }
    output << "},\"features\":[";
    for (size_t index = 0; index < feats.size(); index++)
    {
        if (index != 0) output << ',';
        const Feature &feature = feats[index];
        const FeatureDotsProcessor *carrierTemplateDots = nullptr;
        for (const FeatureDotsProcessor *dots : featureDots)
            if (dots->feat == &feature && dots->normalization == NormalizationCarrierTemplate)
            {
                carrierTemplateDots = dots;
                break;
            }
        output << "{\"feature_index\":" << index;
        output << ",\"rejection_reason\":" << jsonString(featureRejectionReasonCode(feature.rejectionReason));
        output << ",\"accepted\":" << jsonBoolean(feature.valid);
        output << ",\"closed\":" << jsonBoolean(feature.closed);
        output << ",\"original_point_count\":" << feature.originalPointCount;
        output << ",\"decimated_point_count\":" << feature.decimatedPointCount;
        output << ",\"bounds\":{\"min_x\":" << feature.boundsMinX
               << ",\"min_y\":" << feature.boundsMinY
               << ",\"max_x\":" << feature.boundsMaxX
               << ",\"max_y\":" << feature.boundsMaxY
               << ",\"width\":" << feature.boundsWidth
               << ",\"height\":" << feature.boundsHeight << "}";
        output << ",\"aspect_ratio\":" << feature.aspectRatio;
        output << ",\"area_fraction\":" << feature.areaFraction;
        output << ",\"size_check_passed\":" << jsonBoolean(feature.sizeCheckPassed);
        output << ",\"corner_selected\":" << jsonBoolean(feature.cornerValid);
        output << ",\"near_corner_edge_count\":" << feature.nearCornerEdgeCount;
        output << ",\"near_corner_edges\":[";
        for (size_t edgeIndex = 0; edgeIndex < feature.nearCornerEdgeLengths.size(); edgeIndex++)
        {
            if (edgeIndex != 0) output << ',';
            output << "{\"length_pixels\":" << feature.nearCornerEdgeLengths[edgeIndex]
                   << ",\"angle_degrees\":" << feature.nearCornerEdgeAngles[edgeIndex] << '}';
        }
        output << ']';
        output << ",\"corner_angle_difference_degrees\":" << feature.cornerAngleDifference;
        output << ",\"corner_geometry_passed\":" << jsonBoolean(feature.cornerGeometryPassed);
        output << ",\"model_close_point_count\":" << feature.modelClosePointCount;
        output << ",\"model_total_point_count\":" << feature.modelTotalPointCount;
        output << ",\"model_close_fraction\":" << feature.modelCloseFraction;
        output << ",\"outline_validation_passed\":" << jsonBoolean(feature.outlineCheckPassed);
        output << ",\"projective_outline_available\":" << jsonBoolean(feature.projectiveCorrespondenceCount > 0);
        output << ",\"projective_outline_rms_pixels\":" << feature.projectiveRmsError;
        output << ",\"projective_outline_max_error_pixels\":" << feature.projectiveMaxError;
        output << ",\"carrier_projective_class_available\":"
               << jsonBoolean(feature.carrierProjectiveValid);
        output << ",\"carrier_circle_point_count\":" << feature.carrierCirclePointCount;
        output << ",\"carrier_first_edge_point_count\":"
               << feature.carrierFirstEdgePointCount;
        output << ",\"carrier_second_edge_point_count\":"
               << feature.carrierSecondEdgePointCount;
        output << ",\"carrier_projective_rms_pixels\":"
               << feature.carrierProjectiveRmsError;
        output << ",\"carrier_projective_max_error_pixels\":"
               << feature.carrierProjectiveMaxError;
        output << ",\"projective_dot_correspondence_count\":" << feature.projectiveDotCorrespondenceCount;
        output << ",\"projective_dot_rms_pixels\":" << feature.projectiveDotRmsError;
        output << ",\"projective_dot_refined\":" << jsonBoolean(feature.projectiveDotRefined);
        output << ",\"projective_refined_outline_rms_pixels\":"
               << feature.projectiveRefinedOutlineRmsError;
        output << ",\"projective_refined_outline_max_error_pixels\":"
               << feature.projectiveRefinedOutlineMaxError;
        output << ",\"projective_refined_outline_near_fraction\":"
               << feature.projectiveRefinedOutlineNearFraction;
        output << ",\"projective_affine_fallback_used\":" << jsonBoolean(feature.projectiveAffineFallbackUsed);
        output << ",\"projective_affine_fallback_rejected\":" << jsonBoolean(feature.projectiveAffineFallbackRejected);
        output << ",\"affine_normalization_attempted\":" << jsonBoolean(feature.affineNormalizationAttempted);
        output << ",\"projective_normalization_attempted\":" << jsonBoolean(feature.projectiveNormalizationAttempted);
        output << ",\"normalization_available\":"
               << jsonBoolean(feature.affineNormalizationAvailable || feature.projectiveNormalizationAvailable);
        output << ",\"payload_extracted\":"
               << jsonBoolean(feature.affinePayloadExtracted || feature.projectivePayloadExtracted);
        output << ",\"normalization_outcome\":"
               << jsonString(normalizationOutcomeCode(
                   feature, normalization, carrierTemplateDots));
        output << ",\"carrier_template_diagnostics\":";
        if (!carrierTemplateDots)
            output << "null";
        else
        {
            output << "{\"attempted\":"
                   << jsonBoolean(carrierTemplateDots->carrierTemplateAttempted);
            output << ",\"available\":"
                   << jsonBoolean(carrierTemplateDots->carrierTemplateAvailable);
            output << ",\"ambiguity_amount\":"
                   << carrierTemplateDots->carrierTemplateAmbiguityAmount;
            output << ",\"best_loss_pixels\":";
            if (carrierTemplateDots->carrierTemplateBestLoss >= 0)
                output << carrierTemplateDots->carrierTemplateBestLoss;
            else output << "null";
            output << ",\"alternative_payload_loss_pixels\":";
            if (carrierTemplateDots->carrierTemplateAlternativeLoss >= 0)
                output << carrierTemplateDots->carrierTemplateAlternativeLoss;
            else output << "null";
            output << ",\"alternative_payload\":"
                   << (carrierTemplateDots->carrierTemplateAlternativePayload.empty()
                       ? "null" : jsonString(
                           carrierTemplateDots->carrierTemplateAlternativePayload));
            output << ",\"structural_mismatch_pixels\":"
                   << carrierTemplateDots->carrierTemplateStructuralMismatchPixels;
            output << ",\"structural_support_pixels\":"
                   << carrierTemplateDots->carrierTemplateStructuralSupportPixels;
            output << ",\"structural_mismatch_fraction\":";
            if (carrierTemplateDots->carrierTemplateStructuralSupportPixels > 0)
                output << static_cast<double>(
                    carrierTemplateDots->carrierTemplateStructuralMismatchPixels) /
                    carrierTemplateDots->carrierTemplateStructuralSupportPixels;
            else output << "null";
            output << ",\"structural_best_mismatch_pixels\":"
                   << carrierTemplateDots->carrierTemplateStructuralBestMismatchPixels;
            output << ",\"structural_best_amount\":"
                   << carrierTemplateDots->carrierTemplateStructuralBestAmount;
            output << ",\"structural_best_payload\":"
                   << (carrierTemplateDots->carrierTemplateStructuralBestPayload.empty()
                       ? "null" : jsonString(
                           carrierTemplateDots->carrierTemplateStructuralBestPayload));
            output << ",\"structural_consistency_rejected\":"
                   << jsonBoolean(
                       carrierTemplateDots->carrierTemplateStructuralConsistencyRejected);
            output << ",\"structural_alternative_rejected\":"
                   << jsonBoolean(
                       carrierTemplateDots->carrierTemplateStructuralAlternativeRejected);
            output << ",\"phase_audit_attempted\":"
                   << jsonBoolean(carrierTemplateDots->carrierTemplatePhaseAuditAttempted);
            output << ",\"phase_valid_fits\":"
                   << carrierTemplateDots->carrierTemplatePhaseValidFits;
            output << ",\"phase_disagreements\":"
                   << carrierTemplateDots->carrierTemplatePhaseDisagreements;
            output << ",\"raster_confirmation_required\":"
                   << jsonBoolean(
                       carrierTemplateDots->carrierTemplateRasterConfirmationRequired);
            output << ",\"raster_confirmation_attempted\":"
                   << jsonBoolean(
                       carrierTemplateDots->carrierTemplateRasterConfirmationAttempted);
            output << ",\"raster_confirmation_passed\":"
                   << jsonBoolean(
                       carrierTemplateDots->carrierTemplateRasterConfirmationPassed);
            output << ",\"distinct_payloads\":"
                   << carrierTemplateDots->carrierTemplateDistinctPayloads;
            output << ",\"search_steps\":"
                   << carrierTemplateDots->carrierTemplateSearchSteps;
            output << ",\"fitted_payload\":"
                   << (carrierTemplateDots->carrierTemplatePayload.empty()
                       ? "null" : jsonString(carrierTemplateDots->carrierTemplatePayload));
            output << ",\"ambiguous_rejected\":"
                   << jsonBoolean(carrierTemplateDots->carrierTemplateAmbiguous);
            output << ",\"search_boundary_rejected\":"
                   << jsonBoolean(carrierTemplateDots->carrierTemplateBoundaryRejected);
            output << ",\"sampler_disagreement_rejected\":"
                   << jsonBoolean(carrierTemplateDots->carrierTemplateSamplerDisagreed) << '}';
        }
        if (includeVisualGeometry)
        {
            bool detailAvailable = feature.sizeCheckPassed || feature.cornerGeometryPassed ||
                                   feature.outlineCheckPassed || feature.valid;
            output << ",\"visual_geometry\":{\"detail_available\":"
                   << jsonBoolean(detailAvailable);
            output << ",\"coordinate_space\":\"detector_input_pixels\"";
            output << ",\"original_contour_points\":";
            if (detailAvailable) writeFeaturePointsJson(output, feature.origPoints);
            else output << "[]";
            output << ",\"decimated_contour_points\":";
            if (detailAvailable) writeFeaturePointsJson(output, feature.points);
            else output << "[]";
            output << ",\"distinctive_corner\":";
            if (feature.cornerValid)
                output << '[' << feature.cornX << ',' << feature.cornY << ']';
            else
                output << "null";
            output << ",\"corner_edges\":";
            if (feature.edgesValid)
            {
                output << "[{\"from\":[" << feature.cornX << ',' << feature.cornY
                       << "],\"to\":[" << feature.e0.x << ',' << feature.e0.y
                       << "]},{\"from\":[" << feature.cornX << ',' << feature.cornY
                       << "],\"to\":[" << feature.e1.x << ',' << feature.e1.y << "]}]";
            }
            else
                output << "[]";
            output << ",\"far_edge_points\":";
            if (feature.farEdgesValid)
                output << "[[" << feature.far0.x << ',' << feature.far0.y << "],["
                       << feature.far1.x << ',' << feature.far1.y << "]]";
            else
                output << "[]";
            output << ",\"affine_model_to_input\":";
            if (feature.farEdgesValid) writeAffineMatrixJson(output, feature.mat);
            else output << "null";
            output << ",\"projective_model_to_input\":";
            if (feature.projectiveCorrespondenceCount > 0)
                writeProjectiveMatrixJson(output, feature.projectiveMat);
            else
                output << "null";
            output << ",\"carrier_projective_model_to_input\":";
            if (feature.carrierProjectiveValid)
                writeProjectiveMatrixJson(output, feature.carrierProjectiveMat);
            else
                output << "null";
            output << ",\"projective_dot_model_points\":";
            writeProjectivePointsJson(output, feature.projectiveDotModelPoints);
            output << ",\"projective_dot_input_points\":";
            writeProjectivePointsJson(output, feature.projectiveDotImagePoints);
            output << ",\"normalizations\":[";
            bool firstNormalization = true;
            for (const FeatureDotsProcessor *dots : featureDots)
            {
                if (dots->feat != &feature)
                    continue;
                if (!firstNormalization) output << ',';
                firstNormalization = false;
                std::string stem = visualArtifactStem(index, dots);
                const char *strategy = "rejected";
                if (dots->normalizationAvailable)
                {
                    if (dots->normalization == NormalizationAffine)
                        strategy = "affine";
                    else if (dots->normalization == NormalizationCarrierTemplate)
                        strategy = "carrier_template_projective";
                    else if (feature.projectiveDotRefined)
                        strategy = "projective";
                    else if (feature.projectiveAffineFallbackUsed)
                        strategy = fallbackPolicy == QualifiedAffineFallback
                            ? "qualified_affine_fallback" : "legacy_affine_fallback";
                    else
                        strategy = "projective";
                }
                output << "{\"requested\":" << jsonString(normalizationModeCode(dots->normalization));
                output << ",\"strategy\":" << jsonString(strategy);
                output << ",\"available\":" << jsonBoolean(dots->normalizationAvailable);
                output << ",\"observed_bits\":"
                       << (dots->qyooBits.empty() ? "null" : jsonString(dots->qyooBits));
                output << ",\"rectified_patch_file\":";
                if (dots->normalizationAvailable)
                    output << jsonString(stem + "_rectified.png");
                else
                    output << "null";
                output << ",\"affine_pilot_file\":";
                if (dots->affinePilotToInputValid)
                    output << jsonString(stem + "_pilot.png");
                else
                    output << "null";
                output << ",\"normalized_patch_to_input\":";
                if (dots->normalizedPatchToInputValid)
                    writeProjectiveMatrixJson(output, dots->normalizedPatchToInput);
                else
                    output << "null";
                output << ",\"affine_pilot_to_input\":";
                if (dots->affinePilotToInputValid)
                    writeProjectiveMatrixJson(output, dots->affinePilotToInput);
                else
                    output << "null";
                output << ",\"carrier_template\":{";
                output << "\"attempted\":" << jsonBoolean(dots->carrierTemplateAttempted);
                output << ",\"available\":" << jsonBoolean(dots->carrierTemplateAvailable);
                output << ",\"ambiguity_amount\":" << dots->carrierTemplateAmbiguityAmount;
                output << ",\"best_loss_pixels\":";
                if (dots->carrierTemplateBestLoss >= 0) output << dots->carrierTemplateBestLoss;
                else output << "null";
                output << ",\"alternative_payload_loss_pixels\":";
                if (dots->carrierTemplateAlternativeLoss >= 0)
                    output << dots->carrierTemplateAlternativeLoss;
                else output << "null";
                output << ",\"alternative_payload\":"
                       << (dots->carrierTemplateAlternativePayload.empty()
                           ? "null" : jsonString(dots->carrierTemplateAlternativePayload));
                output << ",\"structural_mismatch_pixels\":"
                       << dots->carrierTemplateStructuralMismatchPixels;
                output << ",\"structural_support_pixels\":"
                       << dots->carrierTemplateStructuralSupportPixels;
                output << ",\"structural_mismatch_fraction\":";
                if (dots->carrierTemplateStructuralSupportPixels > 0)
                    output << static_cast<double>(
                        dots->carrierTemplateStructuralMismatchPixels) /
                        dots->carrierTemplateStructuralSupportPixels;
                else output << "null";
                output << ",\"structural_best_mismatch_pixels\":"
                       << dots->carrierTemplateStructuralBestMismatchPixels;
                output << ",\"structural_best_amount\":"
                       << dots->carrierTemplateStructuralBestAmount;
                output << ",\"structural_best_payload\":"
                       << (dots->carrierTemplateStructuralBestPayload.empty()
                           ? "null" : jsonString(dots->carrierTemplateStructuralBestPayload));
                output << ",\"structural_consistency_rejected\":"
                       << jsonBoolean(dots->carrierTemplateStructuralConsistencyRejected);
                output << ",\"structural_alternative_rejected\":"
                       << jsonBoolean(dots->carrierTemplateStructuralAlternativeRejected);
                output << ",\"phase_audit_attempted\":"
                       << jsonBoolean(dots->carrierTemplatePhaseAuditAttempted);
                output << ",\"phase_valid_fits\":"
                       << dots->carrierTemplatePhaseValidFits;
                output << ",\"phase_disagreements\":"
                       << dots->carrierTemplatePhaseDisagreements;
                output << ",\"raster_confirmation_required\":"
                       << jsonBoolean(dots->carrierTemplateRasterConfirmationRequired);
                output << ",\"raster_confirmation_attempted\":"
                       << jsonBoolean(dots->carrierTemplateRasterConfirmationAttempted);
                output << ",\"raster_confirmation_passed\":"
                       << jsonBoolean(dots->carrierTemplateRasterConfirmationPassed);
                output << ",\"distinct_payloads\":" << dots->carrierTemplateDistinctPayloads;
                output << ",\"search_steps\":" << dots->carrierTemplateSearchSteps;
                output << ",\"fitted_payload\":"
                       << (dots->carrierTemplatePayload.empty()
                           ? "null" : jsonString(dots->carrierTemplatePayload));
                output << ",\"ambiguous_rejected\":"
                       << jsonBoolean(dots->carrierTemplateAmbiguous);
                output << ",\"search_boundary_rejected\":"
                       << jsonBoolean(dots->carrierTemplateBoundaryRejected);
                output << ",\"sampler_disagreement_rejected\":"
                       << jsonBoolean(dots->carrierTemplateSamplerDisagreed) << '}';
                output << ",\"patch_size\":{\"width\":" << dots->grayImg->getSizeX()
                       << ",\"height\":" << dots->grayImg->getSizeY() << '}';
                output << ",\"background_sample\":{\"center\":[" << PixelsPerDot / 2
                       << ',' << PixelsPerDot / 2 << "],\"average_gray\":";
                if (dots->backgroundAverage >= 0) output << dots->backgroundAverage;
                else output << "null";
                output << '}';
                output << ",\"sample_region\":{\"shape\":\"strict_disk\",\"radius_pixels\":"
                       << PixelsPerDot / 2 << ",\"cell_pitch_pixels\":" << PixelsPerDot << '}';
                output << ",\"cells\":[";
                bool firstCell = true;
                for (int row = 0; row < QYOOSIZE; row++)
                    for (int position = 0; position < QYOOSIZE; position++)
                    {
                        if (dots->sampledBits[row][position] < 0)
                            continue;
                        if (!firstCell) output << ',';
                        firstCell = false;
                        int bitIndex = (QYOOSIZE - row - 1) * QYOOSIZE +
                                       (QYOOSIZE - position - 1);
                        int centerX = PixelsPerDot * (position + 1) + PixelsPerDot / 2;
                        int centerY = PixelsPerDot * (row + 1) + PixelsPerDot / 2;
                        output << "{\"sampling_row\":" << row
                               << ",\"sampling_column\":" << position
                               << ",\"bit_index\":" << bitIndex
                               << ",\"center\":[" << centerX << ',' << centerY << ']'
                               << ",\"decoded\":" << dots->sampledBits[row][position]
                               << ",\"valid_sample_count\":"
                               << dots->sampleValidCount[row][position]
                               << ",\"matching_sample_count\":"
                               << dots->sampleMatchCount[row][position]
                               << ",\"match_ratio\":"
                               << dots->sampleMatchRatio[row][position]
                               << ",\"decision_boundary\":" << PassRatio
                               << ",\"decision_margin\":"
                               << dots->sampleDecisionMargin[row][position]
                               << ",\"center_gray\":"
                               << dots->sampleCenterGray[row][position]
                               << ",\"mean_gray\":"
                               << dots->sampleMeanGray[row][position]
                               << ",\"median_gray\":"
                               << dots->sampleMedianGray[row][position] << '}';
                    }
                output << "]}";
            }
            output << "]}";
        }
        output << '}';
    }
    output << "]}";
    return output.str();
}
