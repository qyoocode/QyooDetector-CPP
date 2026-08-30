#include "DetectorCore.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <utility>

#include "Logger.h"
#include "QyooModel.h"

namespace
{
class OwnedGdImage
{
public:
    explicit OwnedGdImage(gdImagePtr image = nullptr) : image_(image) { }
    ~OwnedGdImage() { reset(); }

    OwnedGdImage(OwnedGdImage &&that) : image_(that.image_) { that.image_ = nullptr; }
    OwnedGdImage &operator=(OwnedGdImage &&that)
    {
        if (this != &that)
        {
            reset();
            image_ = that.image_;
            that.image_ = nullptr;
        }
        return *this;
    }

    void reset(gdImagePtr image = nullptr)
    {
        if (image_)
            gdImageDestroy(image_);
        image_ = image;
    }

private:
    OwnedGdImage(const OwnedGdImage &);
    OwnedGdImage &operator=(const OwnedGdImage &);
    gdImagePtr image_;
};

struct AttemptSummary
{
    LocalizationScaleAttempt scale;
    int rawFeatureCount;
    int acceptedCandidateCount;
    int payloadCount;
    double runtimeMilliseconds;
};

const double RasterConfirmationRelativeScale = 0.60;

std::string diagnosticJsonString(const std::string &value)
{
    std::ostringstream output;
    output << '"';
    for (unsigned char character : value)
    {
        if (character == '"' || character == '\\') output << '\\' << character;
        else if (character == '\n') output << "\\n";
        else if (character == '\r') output << "\\r";
        else if (character == '\t') output << "\\t";
        else output << character;
    }
    output << '"';
    return output.str();
}

gdImagePtr localizationImage(gdImagePtr original, const LocalizationScaleAttempt &attempt)
{
    if (attempt.width == gdImageSX(original) && attempt.height == gdImageSY(original))
        return original;
    if (!gdImageSetInterpolationMethod(original, GD_LANCZOS3))
        return nullptr;
    return gdImageScale(original, attempt.width, attempt.height);
}

ProjectiveTransform affineTransform(const Feature &feature)
{
    return ProjectiveTransform(
        feature.mat(0, 0), feature.mat(0, 1), feature.mat(0, 2),
        feature.mat(1, 0), feature.mat(1, 1), feature.mat(1, 2),
        feature.mat(2, 0), feature.mat(2, 1), feature.mat(2, 2));
}

bool mapCarrierQuadrilateral(const FeatureDotsProcessor &dots,
                             const LocalizationScaleAttempt &attempt,
                             OriginalImagePoint result[4])
{
    if (!dots.feat || !(attempt.scaleX > 0.0) || !(attempt.scaleY > 0.0))
        return false;
    const Feature &feature = *dots.feat;
    ProjectiveTransform carrierToAttempt;
    if (dots.normalization == NormalizationCarrierTemplate)
    {
        if (!feature.carrierProjectiveValid)
            return false;
        carrierToAttempt = feature.carrierProjectiveMat *
            QyooModel::carrierAmbiguityTransform(dots.carrierTemplateAmbiguityAmount);
    }
    else if (dots.normalization == NormalizationProjective && feature.projectiveDotRefined)
    {
        carrierToAttempt = feature.projectiveMat;
    }
    else
    {
        carrierToAttempt = affineTransform(feature);
    }

    const ProjectivePoint modelCorners[4] = {
        ProjectivePoint(0.0, 0.0), ProjectivePoint(1.0, 0.0),
        ProjectivePoint(1.0, 1.0), ProjectivePoint(0.0, 1.0)
    };
    for (int index = 0; index < 4; ++index)
    {
        ProjectivePoint mapped;
        if (!carrierToAttempt.map(modelCorners[index], mapped))
            return false;
        result[index] = OriginalImagePoint(mapped.x / attempt.scaleX,
                                           mapped.y / attempt.scaleY);
    }
    return true;
}

std::vector<LocalizationCandidate> localizationCandidates(
    const FeatureProcessor &processor, const LocalizationScaleAttempt &attempt,
    int attemptIndex)
{
    std::vector<LocalizationCandidate> candidates;
    for (const FeatureDotsProcessor *dots : processor.featureDots)
    {
        if (dots->qyooBits.empty() || !dots->feat)
            continue;
        const Feature &feature = *dots->feat;
        LocalizationCandidate candidate;
        candidate.payload = dots->qyooBits;
        candidate.bounds = mapBoundsToOriginal(
            feature.boundsMinX, feature.boundsMinY, feature.boundsMaxX, feature.boundsMaxY,
            attempt.scaleX, attempt.scaleY);
        candidate.modelCloseFraction = feature.modelCloseFraction;
        double minimumDimension = std::max(1.0, std::min(
            static_cast<double>(feature.boundsWidth),
            static_cast<double>(feature.boundsHeight)));
        candidate.normalizedCarrierRms = feature.carrierProjectiveRmsError / minimumDimension;
        candidate.carrierTemplateLoss = dots->carrierTemplateBestLoss;
        candidate.carrierTemplateGap = dots->carrierTemplateAlternativeLoss < 0
            ? 0 : dots->carrierTemplateAlternativeLoss - dots->carrierTemplateBestLoss;
        candidate.requiresRasterConfirmation = dots->carrierTemplateRasterConfirmationRequired;
        candidate.carrierQuadrilateralValid = mapCarrierQuadrilateral(
            *dots, attempt, candidate.carrierQuadrilateral);
        candidate.attemptIndex = attemptIndex;
        candidate.featureIndex = static_cast<int>(processor.feats.size());
        for (size_t index = 0; index < processor.feats.size(); ++index)
            if (&processor.feats[index] == dots->feat)
            {
                candidate.featureIndex = static_cast<int>(index);
                break;
            }
        candidates.push_back(candidate);
    }
    return candidates;
}

LocalizationScaleAttempt rasterConfirmationScale(
    int originalWidth, int originalHeight,
    const LocalizationScaleAttempt &acceptedScale)
{
    int width = std::max(1, static_cast<int>(
        acceptedScale.width * RasterConfirmationRelativeScale + 0.5));
    int height = std::max(1, static_cast<int>(
        acceptedScale.height * RasterConfirmationRelativeScale + 0.5));
    return LocalizationScaleAttempt(
        static_cast<double>(width) / originalWidth,
        static_cast<double>(height) / originalHeight,
        width, height, "ambiguity-confirmation-60pct");
}

FeatureDotsProcessor *candidateDots(FeatureProcessor &processor,
                                    const LocalizationCandidate &candidate)
{
    if (candidate.featureIndex < 0 ||
        candidate.featureIndex >= static_cast<int>(processor.feats.size()))
        return nullptr;
    Feature *feature = &processor.feats[candidate.featureIndex];
    for (FeatureDotsProcessor *dots : processor.featureDots)
        if (dots->feat == feature && dots->normalization == NormalizationCarrierTemplate)
            return dots;
    return nullptr;
}

void rejectRasterUnconfirmedCandidate(FeatureDotsProcessor *dots)
{
    if (!dots)
        return;
    dots->qyooBits.clear();
    if (dots->feat)
    {
        dots->feat->dotBits.clear();
        dots->feat->dotBinStr.clear();
        dots->feat->dotDecStr.clear();
        dots->feat->projectivePayloadExtracted = false;
    }
}

std::string appendMultiscaleDiagnostics(
    const std::string &base, const std::string &policy,
    int originalWidth, int originalHeight,
    const std::vector<AttemptSummary> &attempts, int selectedAttempt,
    const std::vector<LocalizationCandidate> &candidates,
    size_t plannedAttemptCount)
{
    if (base.empty() || base.back() != '}')
        return base;
    std::ostringstream output;
    output << base.substr(0, base.size() - 1);
    output << ",\"multiscale\":{\"policy\":" << diagnosticJsonString(policy);
    output << ",\"resampler\":\"libgd_GD_LANCZOS3\"";
    output << ",\"original_image_size\":{\"width\":" << originalWidth
           << ",\"height\":" << originalHeight << "}";
    output << ",\"planned_attempt_count\":" << plannedAttemptCount;
    output << ",\"attempted_scale_count\":" << attempts.size();
    output << ",\"another_scale_attempted\":"
           << (attempts.size() > 1 ? "true" : "false");
    output << ",\"remaining_scales_skipped_after_success\":"
           << (plannedAttemptCount > attempts.size()
               ? plannedAttemptCount - attempts.size() : 0);
    output << ",\"selected_attempt_index\":";
    if (selectedAttempt >= 0) output << selectedAttempt;
    else output << "null";
    output << ",\"attempts\":[";
    for (size_t index = 0; index < attempts.size(); ++index)
    {
        if (index != 0) output << ',';
        const AttemptSummary &attempt = attempts[index];
        output << "{\"attempt_index\":" << index
               << ",\"label\":" << diagnosticJsonString(attempt.scale.label)
               << ",\"input_scale_x\":" << attempt.scale.scaleX
               << ",\"input_scale_y\":" << attempt.scale.scaleY
               << ",\"resampled_width\":" << attempt.scale.width
               << ",\"resampled_height\":" << attempt.scale.height
               << ",\"raw_feature_count\":" << attempt.rawFeatureCount
               << ",\"accepted_candidate_count\":" << attempt.acceptedCandidateCount
               << ",\"payload_count\":" << attempt.payloadCount
               << ",\"runtime_ms\":" << attempt.runtimeMilliseconds << '}';
    }
    output << "] ,\"accepted_candidates_original_coordinates\":[";
    for (size_t index = 0; index < candidates.size(); ++index)
    {
        if (index != 0) output << ',';
        const LocalizationCandidate &candidate = candidates[index];
        output << "{\"feature_index\":" << candidate.featureIndex
               << ",\"payload\":" << diagnosticJsonString(candidate.payload)
               << ",\"bounds\":{\"min_x\":" << candidate.bounds.minX
               << ",\"min_y\":" << candidate.bounds.minY
               << ",\"max_x\":" << candidate.bounds.maxX
               << ",\"max_y\":" << candidate.bounds.maxY << "}}";
    }
    output << "]}}";
    return output.str();
}
}

DetectorCoreResult runDetectorCore(gdImagePtr image, const DetectorCoreOptions &options)
{
    DetectorCoreResult result;
    if (!image || gdImageSX(image) <= 0 || gdImageSY(image) <= 0)
    {
        result.status = DetectorCoreProcessingFailure;
        result.errorMessage = "invalid decoded image";
        return result;
    }

    DetectorLogger logger(options.verbose);
    const int originalWidth = gdImageSX(image);
    const int originalHeight = gdImageSY(image);
    logVerbose(&logger, "Loaded image with size: " + std::to_string(originalWidth) + "x" +
               std::to_string(originalHeight));

    std::vector<LocalizationScaleAttempt> plannedScales;
    if (options.nativeLocalization)
        plannedScales.push_back(LocalizationScaleAttempt(
            1.0, 1.0, originalWidth, originalHeight, "native-debug-control"));
    else
        plannedScales = productionLocalizationScales(originalWidth, originalHeight);

    std::vector<AttemptSummary> attemptSummaries;
    std::unique_ptr<FeatureProcessor> selectedProcessor;
    OwnedGdImage selectedImageOwner;
    gdImagePtr selectedImage = nullptr;
    int selectedAttempt = -1;
    std::vector<LocalizationCandidate> selectedCandidates;
    result.multiscaleRun = plannedScales.size() > 1;

    for (size_t attemptIndex = 0; attemptIndex < plannedScales.size(); ++attemptIndex)
    {
        const LocalizationScaleAttempt &attempt = plannedScales[attemptIndex];
        gdImagePtr processImage = localizationImage(image, attempt);
        if (!processImage)
            continue;
        bool ownsProcessImage = processImage != image;
        OwnedGdImage processImageOwner(ownsProcessImage ? processImage : nullptr);
        std::unique_ptr<FeatureProcessor> processor(
            new FeatureProcessor(processImage, attempt.width, attempt.height, &logger));
        std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
        processor->processImage();
        int found = processor->findQyoo();
        if (found > 0)
            processor->findDots(
                processImage, options.normalization, options.fallbackPolicy,
                options.emitLegacyResults &&
                    options.normalization != NormalizationCarrierTemplate &&
                    !result.multiscaleRun,
                options.writeOutputImages);
        std::vector<LocalizationCandidate> candidates =
            localizationCandidates(*processor, attempt, static_cast<int>(attemptIndex));
        double runtime = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        AttemptSummary summary = {
            attempt, static_cast<int>(processor->feats.size()), processor->numFound,
            static_cast<int>(candidates.size()), runtime};
        attemptSummaries.push_back(summary);

        bool terminal = !candidates.empty() || attemptIndex + 1 == plannedScales.size();
        if (terminal)
        {
            selectedAttempt = candidates.empty() ? -1 : static_cast<int>(attemptIndex);
            selectedCandidates = candidates;
            selectedImage = processImage;
            selectedImageOwner = std::move(processImageOwner);
            selectedProcessor = std::move(processor);
            if (!candidates.empty())
                break;
        }
    }

    if (!selectedProcessor)
    {
        result.status = DetectorCoreProcessingFailure;
        result.errorMessage = "unable to create deterministic localization scale";
        return result;
    }

    bool confirmationAttempted = false;
    if (options.normalization == NormalizationCarrierTemplate &&
        !selectedCandidates.empty() &&
        std::any_of(selectedCandidates.begin(), selectedCandidates.end(),
                    [](const LocalizationCandidate &candidate) {
                        return candidate.requiresRasterConfirmation;
                    }))
    {
        confirmationAttempted = true;
        const LocalizationScaleAttempt &acceptedScale =
            plannedScales[static_cast<size_t>(selectedAttempt)];
        LocalizationScaleAttempt confirmationScale = rasterConfirmationScale(
            originalWidth, originalHeight, acceptedScale);
        gdImagePtr confirmationImage = localizationImage(image, confirmationScale);
        std::vector<LocalizationCandidate> confirmationCandidates;
        if (confirmationImage)
        {
            bool ownsConfirmationImage = confirmationImage != image;
            OwnedGdImage confirmationImageOwner(
                ownsConfirmationImage ? confirmationImage : nullptr);
            FeatureProcessor confirmationProcessor(
                confirmationImage, confirmationScale.width, confirmationScale.height, &logger);
            std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
            confirmationProcessor.processImage();
            int found = confirmationProcessor.findQyoo();
            if (found > 0)
                confirmationProcessor.findDots(
                    confirmationImage, options.normalization, options.fallbackPolicy,
                    false, options.writeOutputImages);
            confirmationCandidates = localizationCandidates(
                confirmationProcessor, confirmationScale,
                static_cast<int>(attemptSummaries.size()));
            double runtime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count();
            AttemptSummary summary = {
                confirmationScale, static_cast<int>(confirmationProcessor.feats.size()),
                confirmationProcessor.numFound,
                static_cast<int>(confirmationCandidates.size()), runtime};
            attemptSummaries.push_back(summary);
        }

        std::vector<LocalizationCandidate> qualified;
        for (const LocalizationCandidate &candidate : selectedCandidates)
        {
            if (!candidate.requiresRasterConfirmation)
            {
                qualified.push_back(candidate);
                continue;
            }
            FeatureDotsProcessor *dots = candidateDots(*selectedProcessor, candidate);
            if (dots)
                dots->carrierTemplateRasterConfirmationAttempted = true;
            bool samePayload = false;
            bool conflictingPayload = false;
            for (const LocalizationCandidate &confirmation : confirmationCandidates)
            {
                if (!samePhysicalCarrier(candidate, confirmation))
                    continue;
                if (confirmation.payload == candidate.payload)
                    samePayload = true;
                else
                    conflictingPayload = true;
            }
            bool passed = samePayload && !conflictingPayload;
            if (dots)
                dots->carrierTemplateRasterConfirmationPassed = passed;
            if (passed)
                qualified.push_back(candidate);
            else
                rejectRasterUnconfirmedCandidate(dots);
        }
        selectedCandidates = qualified;
        if (selectedCandidates.empty())
            selectedAttempt = -1;
    }

    if (options.diagnostics)
    {
        if (!options.visualDebugDirectory.empty())
            result.visualDebugArtifactsWritten = selectedProcessor->writeVisualDebugArtifacts(
                selectedImage, options.visualDebugDirectory);
        std::string report = selectedProcessor->diagnosticsJson(
            options.imageId, options.normalization, options.fallbackPolicy,
            !options.visualDebugDirectory.empty());
        result.diagnosticsJson = appendMultiscaleDiagnostics(
            report, options.nativeLocalization ? "native-debug-control" : "bounded-camera-v1",
            originalWidth, originalHeight, attemptSummaries, selectedAttempt,
            selectedCandidates,
            plannedScales.size() + (confirmationAttempted ? 1 : 0));
    }

    result.candidates = selectedCandidates;
    return result;
}
