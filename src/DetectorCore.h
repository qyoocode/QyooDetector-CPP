#pragma once

#include <string>
#include <vector>

#include <gd.h>

#include "FeatureDetector.h"
#include "MultiscaleLocalization.h"

enum DetectorCoreStatus
{
    DetectorCoreSuccess,
    DetectorCoreProcessingFailure
};

struct DetectorCoreOptions
{
    DetectorCoreOptions()
        : normalization(NormalizationCarrierTemplate),
          fallbackPolicy(QualifiedAffineFallback), nativeLocalization(false),
          diagnostics(false), verbose(false), emitLegacyResults(false),
          writeOutputImages(false) { }

    NormalizationMode normalization;
    ProjectiveFallbackPolicy fallbackPolicy;
    bool nativeLocalization;
    bool diagnostics;
    bool verbose;
    bool emitLegacyResults;
    bool writeOutputImages;
    std::string imageId;
    std::string visualDebugDirectory;
};

struct DetectorCoreResult
{
    DetectorCoreResult()
        : status(DetectorCoreSuccess), multiscaleRun(false),
          visualDebugArtifactsWritten(true) { }

    DetectorCoreStatus status;
    bool multiscaleRun;
    bool visualDebugArtifactsWritten;
    std::string errorMessage;
    std::string diagnosticsJson;
    std::vector<LocalizationCandidate> candidates;
};

// Runs the frozen production detector against an already decoded GD image.
// The caller retains ownership of image. This header is private to the library
// and CLI; portable clients use include/qyoo_detector.h instead.
DetectorCoreResult runDetectorCore(gdImagePtr image, const DetectorCoreOptions &options);
