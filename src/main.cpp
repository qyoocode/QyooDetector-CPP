#include <algorithm>
#include <iostream>
#include <chrono>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <gd.h>
#include "FeatureDetector.h"
#include "ImageLoader.h"
#include "MultiscaleLocalization.h"

// Global verbose flag for controlling debug output
bool verbose = false;

// Function to handle verbose logging
void logVerbose(const std::string& message) {
    if (verbose) {
        std::cout << "Debug: " << message << std::endl;
    }
}

static std::string diagnosticJsonString(const std::string &value) {
    std::ostringstream output;
    output << '"';
    for (unsigned char character : value) {
        if (character == '"' || character == '\\') output << '\\' << character;
        else if (character == '\n') output << "\\n";
        else if (character == '\r') output << "\\r";
        else if (character == '\t') output << "\\t";
        else output << character;
    }
    output << '"';
    return output.str();
}

struct AttemptSummary {
    LocalizationScaleAttempt scale;
    int rawFeatureCount;
    int acceptedCandidateCount;
    int payloadCount;
    double runtimeMilliseconds;
};

static const double RasterConfirmationRelativeScale = 0.60;

static gdImagePtr localizationImage(gdImagePtr original,
                                    const LocalizationScaleAttempt &attempt) {
    if (attempt.width == gdImageSX(original) && attempt.height == gdImageSY(original))
        return original;
    if (!gdImageSetInterpolationMethod(original, GD_LANCZOS3))
        return nullptr;
    return gdImageScale(original, attempt.width, attempt.height);
}

static std::vector<LocalizationCandidate> localizationCandidates(
    const FeatureProcessor &processor, const LocalizationScaleAttempt &attempt,
    int attemptIndex) {
    std::vector<LocalizationCandidate> candidates;
    for (const FeatureDotsProcessor *dots : processor.featureDots) {
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
        candidate.requiresRasterConfirmation =
            dots->carrierTemplateRasterConfirmationRequired;
        candidate.attemptIndex = attemptIndex;
        candidate.featureIndex = static_cast<int>(processor.feats.size());
        for (size_t index = 0; index < processor.feats.size(); index++)
            if (&processor.feats[index] == dots->feat) {
                candidate.featureIndex = static_cast<int>(index);
                break;
            }
        candidates.push_back(candidate);
    }
    return candidates;
}

static LocalizationScaleAttempt rasterConfirmationScale(
    int originalWidth, int originalHeight,
    const LocalizationScaleAttempt &acceptedScale) {
    int width = std::max(1, static_cast<int>(
        acceptedScale.width * RasterConfirmationRelativeScale + 0.5));
    int height = std::max(1, static_cast<int>(
        acceptedScale.height * RasterConfirmationRelativeScale + 0.5));
    return LocalizationScaleAttempt(
        static_cast<double>(width) / originalWidth,
        static_cast<double>(height) / originalHeight,
        width, height, "ambiguity-confirmation-60pct");
}

static FeatureDotsProcessor *candidateDots(FeatureProcessor &processor,
                                            const LocalizationCandidate &candidate) {
    if (candidate.featureIndex < 0 ||
        candidate.featureIndex >= static_cast<int>(processor.feats.size()))
        return nullptr;
    Feature *feature = &processor.feats[candidate.featureIndex];
    for (FeatureDotsProcessor *dots : processor.featureDots)
        if (dots->feat == feature && dots->normalization == NormalizationCarrierTemplate)
            return dots;
    return nullptr;
}

static void rejectRasterUnconfirmedCandidate(FeatureDotsProcessor *dots) {
    if (!dots)
        return;
    dots->qyooBits.clear();
    if (dots->feat) {
        dots->feat->dotBits.clear();
        dots->feat->dotBinStr.clear();
        dots->feat->dotDecStr.clear();
        dots->feat->projectivePayloadExtracted = false;
    }
}

static std::string appendMultiscaleDiagnostics(
    const std::string &base, const std::string &policy,
    int originalWidth, int originalHeight,
    const std::vector<AttemptSummary> &attempts, int selectedAttempt,
    const std::vector<LocalizationCandidate> &candidates,
    size_t plannedAttemptCount) {
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
    for (size_t index = 0; index < attempts.size(); index++) {
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
    for (size_t index = 0; index < candidates.size(); index++) {
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

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <image_file> [--v|--verbose] "
                  << "[--normalization affine|projective|carrier-template|shadow] "
                  << "[--fallback-policy legacy-affine|reject|qualified] [--diagnostics] "
                  << "[--visual-debug-dir DIRECTORY] "
                  << "[--localization-policy production|native]" << std::endl;
        return 1;
    }

    // Check if verbose flag is set
    NormalizationMode normalization = NormalizationCarrierTemplate;
    ProjectiveFallbackPolicy fallbackPolicy = QualifiedAffineFallback;
    bool diagnostics = false;
    bool nativeLocalization = false;
    std::string visualDebugDirectory;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--v" || arg == "--verbose") {
            verbose = true;  // Enable verbose logging
        } else if (arg == "--diagnostics") {
            diagnostics = true;
        } else if (arg == "--visual-debug-dir") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --visual-debug-dir requires an existing directory." << std::endl;
                return 1;
            }
            visualDebugDirectory = argv[++i];
            diagnostics = true;
        } else if (arg == "--normalization") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --normalization requires affine, projective, carrier-template, or shadow." << std::endl;
                return 1;
            }
            std::string value = argv[++i];
            if (value == "affine") normalization = NormalizationAffine;
            else if (value == "projective") normalization = NormalizationProjective;
            else if (value == "carrier-template") normalization = NormalizationCarrierTemplate;
            else if (value == "shadow") normalization = NormalizationShadow;
            else {
                std::cerr << "Error: unknown normalization mode: " << value << std::endl;
                return 1;
            }
        } else if (arg == "--fallback-policy") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --fallback-policy requires legacy-affine, reject, or qualified." << std::endl;
                return 1;
            }
            std::string value = argv[++i];
            if (value == "legacy-affine") fallbackPolicy = LegacyAffineFallback;
            else if (value == "reject") fallbackPolicy = RejectUnsupportedProjective;
            else if (value == "qualified") fallbackPolicy = QualifiedAffineFallback;
            else {
                std::cerr << "Error: unknown fallback policy: " << value << std::endl;
                return 1;
            }
        } else if (arg == "--localization-policy") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --localization-policy requires production or native."
                          << std::endl;
                return 1;
            }
            std::string value = argv[++i];
            if (value == "production") nativeLocalization = false;
            else if (value == "native") nativeLocalization = true;
            else {
                std::cerr << "Error: unknown localization policy: " << value << std::endl;
                return 1;
            }
        }
    }

    std::string image_file = argv[1]; // The first argument should be the image file

    // Load the image (using gdImagePtr)
    gdImagePtr theImage = loadImage(image_file);
    if (!theImage) {
        if (diagnostics) {
            std::cout << "Diagnostics JSON = {\"schema\":\"org.qyoo.detector.rejection-diagnostics\","
                      << "\"schema_version\":1,\"image_id\":" << diagnosticJsonString(image_file)
                      << ",\"image_loaded\":false,\"failure_stage\":\"image_load_preparation\"}"
                      << std::endl;
        }
        return 1; // Exit if image loading fails
    }

    // Convert palette-based image to true color if necessary
    if (!gdImageTrueColor(theImage)) {
        gdImagePtr trueColorImg = gdImageCreateTrueColor(gdImageSX(theImage), gdImageSY(theImage));
        if (!trueColorImg) {
            std::cerr << "Error: Unable to create true color image." << std::endl;
            gdImageDestroy(theImage);
            return 1;
        }

        // Copy the palette-based image into the true color image
        gdImageCopy(trueColorImg, theImage, 0, 0, 0, 0, gdImageSX(theImage), gdImageSY(theImage));
        gdImageDestroy(theImage);  // Clean up the original palette-based image
        theImage = trueColorImg;   // Replace with the true-color image
    }

    int originalWidth = gdImageSX(theImage);
    int originalHeight = gdImageSY(theImage);
    logVerbose("Loaded image with size: " + std::to_string(originalWidth) + "x" +
               std::to_string(originalHeight));

    std::vector<LocalizationScaleAttempt> plannedScales;
    if (nativeLocalization)
        plannedScales.push_back(LocalizationScaleAttempt(
            1.0, 1.0, originalWidth, originalHeight, "native-debug-control"));
    else
        plannedScales = productionLocalizationScales(originalWidth, originalHeight);

    std::vector<AttemptSummary> attemptSummaries;
    std::unique_ptr<FeatureProcessor> selectedProcessor;
    gdImagePtr selectedImage = nullptr;
    bool selectedImageOwned = false;
    int selectedAttempt = -1;
    std::vector<LocalizationCandidate> selectedCandidates;
    bool multiscaleRun = plannedScales.size() > 1;

    for (size_t attemptIndex = 0; attemptIndex < plannedScales.size(); attemptIndex++) {
        const LocalizationScaleAttempt &attempt = plannedScales[attemptIndex];
        gdImagePtr processImage = localizationImage(theImage, attempt);
        if (!processImage) {
            std::cerr << "Error: unable to create deterministic localization scale "
                      << attempt.label << "." << std::endl;
            continue;
        }
        bool ownsProcessImage = processImage != theImage;
        std::unique_ptr<FeatureProcessor> processor(
            new FeatureProcessor(processImage, attempt.width, attempt.height));
        std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
        processor->processImage();
        int found = processor->findQyoo();
        if (found > 0)
            processor->findDots(
                processImage, normalization, fallbackPolicy,
                normalization != NormalizationCarrierTemplate && !multiscaleRun);
        std::vector<LocalizationCandidate> candidates =
            localizationCandidates(*processor, attempt, static_cast<int>(attemptIndex));
        double runtime = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        AttemptSummary summary = {attempt, static_cast<int>(processor->feats.size()),
                                  processor->numFound, static_cast<int>(candidates.size()), runtime};
        attemptSummaries.push_back(summary);

        bool terminal = !candidates.empty() || attemptIndex + 1 == plannedScales.size();
        if (terminal) {
            selectedAttempt = candidates.empty() ? -1 : static_cast<int>(attemptIndex);
            selectedCandidates = candidates;
            selectedImage = processImage;
            selectedImageOwned = ownsProcessImage;
            selectedProcessor = std::move(processor);
            if (!candidates.empty())
                break; // A deterministically accepted first-tier result is the early-exit gate.
        } else if (ownsProcessImage) {
            gdImageDestroy(processImage);
        }
    }

    if (!selectedProcessor) {
        std::cerr << "No Qyoo found in the image." << std::endl;
        gdImageDestroy(theImage);
        return 1;
    }

    bool confirmationAttempted = false;
    if (normalization == NormalizationCarrierTemplate && !selectedCandidates.empty() &&
        std::any_of(selectedCandidates.begin(), selectedCandidates.end(),
                    [](const LocalizationCandidate &candidate) {
                        return candidate.requiresRasterConfirmation;
                    })) {
        confirmationAttempted = true;
        const LocalizationScaleAttempt &acceptedScale =
            plannedScales[static_cast<size_t>(selectedAttempt)];
        LocalizationScaleAttempt confirmationScale = rasterConfirmationScale(
            originalWidth, originalHeight, acceptedScale);
        gdImagePtr confirmationImage = localizationImage(theImage, confirmationScale);
        std::vector<LocalizationCandidate> confirmationCandidates;
        if (confirmationImage) {
            bool ownsConfirmationImage = confirmationImage != theImage;
            FeatureProcessor confirmationProcessor(
                confirmationImage, confirmationScale.width, confirmationScale.height);
            std::chrono::steady_clock::time_point started =
                std::chrono::steady_clock::now();
            confirmationProcessor.processImage();
            int found = confirmationProcessor.findQyoo();
            if (found > 0)
                confirmationProcessor.findDots(
                    confirmationImage, normalization, fallbackPolicy, false);
            confirmationCandidates = localizationCandidates(
                confirmationProcessor, confirmationScale,
                static_cast<int>(attemptSummaries.size()));
            double runtime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count();
            AttemptSummary summary = {
                confirmationScale,
                static_cast<int>(confirmationProcessor.feats.size()),
                confirmationProcessor.numFound,
                static_cast<int>(confirmationCandidates.size()), runtime};
            attemptSummaries.push_back(summary);
            if (ownsConfirmationImage)
                gdImageDestroy(confirmationImage);
        }

        std::vector<LocalizationCandidate> qualified;
        for (const LocalizationCandidate &candidate : selectedCandidates) {
            if (!candidate.requiresRasterConfirmation) {
                qualified.push_back(candidate);
                continue;
            }
            FeatureDotsProcessor *dots = candidateDots(*selectedProcessor, candidate);
            if (dots)
                dots->carrierTemplateRasterConfirmationAttempted = true;
            bool samePayload = false;
            bool conflictingPayload = false;
            for (const LocalizationCandidate &confirmation : confirmationCandidates) {
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

    if ((multiscaleRun || normalization == NormalizationCarrierTemplate) &&
        !selectedCandidates.empty()) {
        for (const LocalizationCandidate &candidate : selectedCandidates) {
            std::cout << "Binary = " << candidate.payload << std::endl;
            std::cout << "Qyoo value = " << std::stoull(candidate.payload, nullptr, 2)
                      << std::endl;
        }
    }
    if (selectedCandidates.empty())
        std::cerr << "No Qyoo found in the image." << std::endl;
    else
        logVerbose("Feature processing completed successfully.");

    if (diagnostics)
    {
        if (!visualDebugDirectory.empty() &&
            !selectedProcessor->writeVisualDebugArtifacts(selectedImage, visualDebugDirectory))
            std::cerr << "Error: one or more visual debug artifacts could not be written." << std::endl;
        std::string report = selectedProcessor->diagnosticsJson(
            image_file, normalization, fallbackPolicy, !visualDebugDirectory.empty());
        report = appendMultiscaleDiagnostics(
            report, nativeLocalization ? "native-debug-control" : "bounded-camera-v1",
            originalWidth, originalHeight, attemptSummaries, selectedAttempt,
            selectedCandidates,
            plannedScales.size() + (confirmationAttempted ? 1 : 0));
        std::cout << "Diagnostics JSON = " << report << std::endl;
    }

    // Clean up
    if (selectedImageOwned)
        gdImageDestroy(selectedImage);
    gdImageDestroy(theImage); // Destroy the image to avoid memory leaks

    return 0;
}
