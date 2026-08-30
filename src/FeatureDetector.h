/*
 *  FeatureDetector.h
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/20/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 *  This header file defines the classes and methods for detecting Qyoo features
 *  and processing dots in an image. It includes the main classes `FeatureProcessor`
 *  and `FeatureDotsProcessor`, which handle feature detection and dot analysis.
 */

#import "RawImage.h"
#import "Convolution.h"
#import "CannyDetector.h"
#import "Feature.h"

class FeatureProcessor;

// Number of rows/columns in a Qyoo
#define QYOOSIZE 6

enum NormalizationMode
{
  NormalizationAffine,
  NormalizationProjective,
  NormalizationCarrierTemplate,
  NormalizationShadow
};

enum ProjectiveFallbackPolicy
{
  LegacyAffineFallback,
  RejectUnsupportedProjective,
  QualifiedAffineFallback
};

/*
 * FeatureDotsProcessor
 * This class handles the detection and processing of dots for a single feature.
 * It extracts the relevant dot data and stores it in the corresponding feature object.
 */
class FeatureDotsProcessor
{
 public:
  // Constructor: Initializes the processor with the given image, feature processor, and feature.
  FeatureDotsProcessor(gdImagePtr inImage, FeatureProcessor *featProc, Feature *feat,
                       NormalizationMode normalization = NormalizationAffine,
                       const std::string &resultLabel = "",
                       ProjectiveFallbackPolicy fallbackPolicy = QualifiedAffineFallback,
                       bool emitResult = true);

  // Destructor: Cleans up resources used by the processor.
  ~FeatureDotsProcessor();

  // Look for dots in the feature and store them in the feature object.
  // This version uses raw grayscale data for processing.
  void findDotsGray();

protected:
  // Initialize the processor with the given image, feature processor, and feature.
  void init(gdImagePtr inImage, FeatureProcessor *featProc, Feature *feat,
            NormalizationMode normalization, const std::string &resultLabel,
            ProjectiveFallbackPolicy fallbackPolicy, bool emitResult);

 public:
  RawImageGray8 *grayImg;   // Grayscale version of the image
  RawImageGray8 *gaussImg;  // Gaussian blurred image (not used in this version)
  RawImageGray32 *gradImg;  // Gradient image (not used in this version)
  FeatureProcessor *featProc;  // Pointer to the feature processor
  Feature *feat;              // The feature being processed for dot detection

  // Detected Qyoo bits in string format
  std::string qyooBits;

  // Array of detected Qyoo rows
  int qyooRows[QYOOSIZE];
  int sampledBits[QYOOSIZE][QYOOSIZE];
  int sampleValidCount[QYOOSIZE][QYOOSIZE];
  int sampleMatchCount[QYOOSIZE][QYOOSIZE];
  int sampleCenterGray[QYOOSIZE][QYOOSIZE];
  double sampleMatchRatio[QYOOSIZE][QYOOSIZE];
  double sampleMeanGray[QYOOSIZE][QYOOSIZE];
  double sampleMedianGray[QYOOSIZE][QYOOSIZE];
  double sampleDecisionMargin[QYOOSIZE][QYOOSIZE];
  int backgroundAverage;
  NormalizationMode normalization;
  std::string resultLabel;
  bool normalizationAvailable;
  ProjectiveFallbackPolicy fallbackPolicy;
  ProjectiveTransform normalizedPatchToInput;
  bool normalizedPatchToInputValid;
  ProjectiveTransform affinePilotToInput;
  bool affinePilotToInputValid;
  bool carrierTemplateAttempted;
  bool carrierTemplateAvailable;
  bool carrierTemplateAmbiguous;
  bool carrierTemplateBoundaryRejected;
  bool carrierTemplateSamplerDisagreed;
  double carrierTemplateAmbiguityAmount;
  int carrierTemplateBestLoss;
  int carrierTemplateAlternativeLoss;
  std::string carrierTemplateAlternativePayload;
  int carrierTemplateStructuralMismatchPixels;
  int carrierTemplateStructuralSupportPixels;
  int carrierTemplateStructuralBestMismatchPixels;
  double carrierTemplateStructuralBestAmount;
  std::string carrierTemplateStructuralBestPayload;
  bool carrierTemplateStructuralConsistencyRejected;
  bool carrierTemplateStructuralAlternativeRejected;
  bool carrierTemplatePhaseAuditAttempted;
  int carrierTemplatePhaseValidFits;
  int carrierTemplatePhaseDisagreements;
  bool carrierTemplateRasterConfirmationRequired;
  bool carrierTemplateRasterConfirmationAttempted;
  bool carrierTemplateRasterConfirmationPassed;
  int carrierTemplateDistinctPayloads;
  int carrierTemplateSearchSteps;
  std::string carrierTemplatePayload;
  bool emitResult;
};

/*
 * FeatureProcessor
 * This class manages the entire image processing pass, from edge detection to feature
 * analysis and dot detection. It handles the processing of features and identifying
 * valid Qyoo shapes.
 */
class FeatureProcessor
{
 public:
  // Constructor: Initializes the processor with the given image and image size.
  FeatureProcessor(gdImagePtr inImage, int processSizeX, int processSizeY);

  // Destructor: Cleans up resources used by the processor.
  ~FeatureProcessor();

  // Processes the image up to the point of finding thin edges and gradients.
  void processImage();

  // Reprocess the image using a larger gradient (for debugging purposes).
  void redoGradient(gdImagePtr inImage, int processSizeX, int processSizeY);

  // Detect Qyoo features in the processed image.
  // Returns the number of valid Qyoo features found.
  int findQyoo();

  // Find and process the dots in the valid Qyoo features.
  void findDots(gdImagePtr inImage, NormalizationMode normalization = NormalizationCarrierTemplate,
                ProjectiveFallbackPolicy fallbackPolicy = QualifiedAffineFallback,
                bool emitResults = true);

  // Optional decision-neutral diagnostics for test/recovery harnesses.
  std::string diagnosticsJson(const std::string &imageId,
                              NormalizationMode normalization,
                              ProjectiveFallbackPolicy fallbackPolicy,
                              bool includeVisualGeometry = false) const;

  // Write exact grayscale patches after all detector decisions have completed.
  // This is opt-in instrumentation and is never consulted by detection.
  bool writeVisualDebugArtifacts(gdImagePtr inImage,
                                 const std::string &outputDirectory) const;

 public:
  ConvolutionFilterInt *gaussFilter;  // Gaussian filter to reduce noise in the image
  RawImageGray8 *grayImg;             // Grayscale version of the input image
  RawImageGray8 *gaussImg;            // Gaussian blurred image
  RawImageGray32 *gradImg;            // Gradient image (calculated during edge detection)
  RawImageGray8 *thetaImg;            // Angle of the edges in the image
  RawImageGray32 *featImg;            // Feature map used to mark off detected features

  std::vector<Feature> feats;         // List of detected features
  int numFound;                       // Number of valid Qyoo features found

  // List of processors for the detected dots in valid Qyoo features
  std::vector<FeatureDotsProcessor *> featureDots;
};
