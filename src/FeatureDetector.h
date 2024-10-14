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

/*
 * FeatureDotsProcessor
 * This class handles the detection and processing of dots for a single feature.
 * It extracts the relevant dot data and stores it in the corresponding feature object.
 */
class FeatureDotsProcessor
{
 public:
  // Constructor: Initializes the processor with the given image, feature processor, and feature.
  FeatureDotsProcessor(gdImagePtr inImage, FeatureProcessor *featProc, Feature *feat);

  // Destructor: Cleans up resources used by the processor.
  ~FeatureDotsProcessor();

  // Look for dots in the feature and store them in the feature object.
  // This version uses raw grayscale data for processing.
  void findDotsGray();

protected:
  // Initialize the processor with the given image, feature processor, and feature.
  void init(gdImagePtr inImage, FeatureProcessor *featProc, Feature *feat);

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

  void saveDebugImages(const std::string& baseFilename, float angle);

  // Processes the image up to the point of finding thin edges and gradients.
  void processImage();

  // Reprocess the image using a larger gradient (for debugging purposes).
  void redoGradient(gdImagePtr inImage, int processSizeX, int processSizeY);

  // Detect Qyoo features in the processed image.
  // Returns the number of valid Qyoo features found.
  int findQyoo();

  // Find and process the dots in the valid Qyoo features.
  void findDots(gdImagePtr inImage);

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

