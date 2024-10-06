/*
 *  CannyDectector.h
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/15/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#import "RawImage.h"
#import "Convolution.h"
#import "Feature.h"

typedef enum {ThetaEmpty=0,Theta0,Theta45,Theta90,Theta135} ThetaAngles;
// Calculate the gradient magnitude and direction at each pixel
void CannyGradientAndTheta(RawImageGray8 *gaussImg,RawImageGray32 *gradImg,RawImageGray8 *thetaImg);

#define CannyThinFlag (1<<7)
// Run non-maximal supression
// Anything below the threshhold is nuked
// Edges that are at the "top" of their gradient will be marked as Thin
void CannyNonMaxSupress(RawImageGray32 *gradImg,RawImageGray8 *thetaImg,int gradThresh);

// Find features (in a really simple way)
void CannyFindFeatures(RawImageGray32 *gradImg,RawImageGray8 *thetaImg,int minThresh,int maxThresh,std::vector<Feature> &feats,RawImageGray32 *featImg);

void calcNextGridDir(int offset,int cx,int cy,int &nx,int &ny,int &gridDir);
bool pixelCrowded(RawImageGray32 *featImg,int cx,int cy);
void ScrubFeature(RawImageGray8 *featImg,int featId);
bool closedFeature(RawImageGray32 *featImg,int featId,int cx,int cy,int gridDir);
void calcNextThetaDir(ThetaAngles dir,int offset,int cx,int cy,int &nx,int &ny,int &gridDir);
