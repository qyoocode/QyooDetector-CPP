/*
 *  FeatureDetector.h
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/20/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#import "RawImage.h"
#import "Convolution.h"
#import "CannyDetector.h"
#import "Feature.h"

class FeatureProcessor;

// Note: Probably shouldn't be hard coded
#define QYOOSIZE 6

/* Feature Dots Processor
	This holds the results of dot processing for a single feature.
 */
class FeatureDotsProcessor
{
 public:
#ifdef QYOO_CMD
  FeatureDotsProcessor(gdImagePtr inImage,FeatureProcessor *featProc,Feature *feat);
#else
  FeatureDotsProcessor(UIImage *inImage, FeatureProcessor *featProc,Feature *feat, bool flip, bool vertFlip);
#endif
  ~FeatureDotsProcessor();

  // Look for the dots and store the results in the feature
  // This version uses the gradient image
  // Note: This version is out of date
//  void findDotsGradient();
	
  // This version uses just the raw grayscale data
  void findDotsGray();


protected:
#ifdef QYOO_CMD
  void init(gdImagePtr,FeatureProcessor *,Feature *);
#else
  void init(UIImage *inImage, FeatureProcessor *featProc, Feature *feat, bool flip, bool vertFlip);
#endif

 public:
	RawImageGray8 *grayImg;  // Grayscale version of image
	RawImageGray8 *gaussImg; // Gauss blurred image
	RawImageGray32 *gradImg; // Gradient image
	FeatureProcessor *featProc;
	Feature *feat;           // The feature we're looking at
	
	// JBB: Added the following:
	std::string qyooBits;
	int qyooRows [QYOOSIZE];
};

/* Processing Pass Data
	This holds the data for an entire processing pass.
 */
class FeatureProcessor
{
 public:
#ifdef QYOO_CMD
  FeatureProcessor(gdImagePtr inImage,int processSizeX, int processSizeY);
#else
  FeatureProcessor(UIImage *inImage, int processSizeX, int processSizeY, bool flip, bool vertFlip);
#endif
  ~FeatureProcessor();

  // Do the image processing up to finding thin edges
  void processImage();
  // Redo the gradient image in a larger size (for debugging)
#ifdef QYOO_CMD
  void redoGradient(gdImagePtr inImage, int processSizeX, int processSizeY);
#else
  void redoGradient(UIImage *inImage, int processSizeX, int processSizeY, bool flip, bool vertFlip);
#endif

  // Switch to feature based processing
  // Returns the number of Qyoo's found
  int findQyoo();
  // For the valid qyoo, find their dots
  // We pass in the size of the image the features were found in
#ifdef QYOO_CMD
  void findDots(gdImagePtr inImage);
#else
  void findDots(UIImage *inImage, bool flip, bool vertFlip);
#endif

 public:
	ConvolutionFilterInt *gaussFilter;  // The filter we'll use to reduce noise
	RawImageGray8 *grayImg;          // First thing we do is downsample and convert to grayscale
	RawImageGray8 *gaussImg;         // The gauss blurred image
	RawImageGray32 *gradImg;         // Gradient image
	RawImageGray8 *thetaImg;         // Angles and thin edges
	RawImageGray32 *featImg;         // Used to mark off features as we follow them
	std::vector<Feature> feats;
	int numFound;                    // If we've done it, the number of Qyoo found

	std::vector<FeatureDotsProcessor *> featureDots;
//	NSMutableArray *featureDots;     // Data used to tease out the dots in individual qyoos
};
