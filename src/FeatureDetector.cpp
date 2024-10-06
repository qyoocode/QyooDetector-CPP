/*
 *  FeatureDetector.mm
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/20/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#include <iostream>
#include <sstream>

#import "FeatureDetector.h"
#import "QyooModel.h"

// Pixels per dot for detection
const int PixelsPerDot = 11;

// Set up with a grayscale image
FeatureDotsProcessor::FeatureDotsProcessor(gdImagePtr inImage,FeatureProcessor *inFeatProc,Feature *inFeat)
{
  init(inImage,inFeatProc,inFeat);
}

void FeatureDotsProcessor::init(gdImagePtr inImage,FeatureProcessor *inFeatProc,Feature *inFeat)
{
	grayImg = NULL;
	gaussImg = NULL;
	gradImg = NULL;
	featProc = NULL;
	feat = NULL;
	
  qyooModel *qyooModel = qyooModel::getQyooModel();
  
  featProc = inFeatProc;
  feat = inFeat;
  
  //  CGImageRef imgRef = inImage.CGImage;
  //  CGFloat imgWidth = CGImageGetWidth(imgRef);
  //  CGFloat imgHeight = CGImageGetHeight(imgRef);
  float imgWidth = gdImageSX(inImage);
  float imgHeight = gdImageSY(inImage);
    
  // Size of the image we want to render to
  int sizeX = (PixelsPerDot) * (qyooModel->numRows() + 2);
  int sizeY = (PixelsPerDot) * (qyooModel->numPos() + 2);

  // This transform will get us from the feature pixel space to unit qyoo space
  float scaleX = (float)imgWidth/(float)feat->imgSizeX, scaleY = (float)imgHeight/(float)feat->imgSizeY;
  QyooMatrix scaleMat(scaleX, 0.0, 0.0,
		      0.0, scaleY, 0.0,
		      0.0, 0.0, 1.0);
  QyooMatrix forMat = scaleMat * feat->mat;

  // Focus on just the dots
  // But add in a dots worth of space around
  SimplePoint2D ll,ur;
  qyooModel->dotBounds(ll,ur,true);
  QyooMatrix transMat(1.0, 0.0, ll.x,
		      0.0, 1.0, ll.y,
		      0.0, 0.0, 1.0);
  QyooMatrix scaleMat2(ur.x-ll.x, 0.0, 0.0,
		      0.0, ur.y-ll.y, 0.0,
		      0.0, 0.0, 1.0);
  forMat = forMat * transMat;
  forMat = forMat * scaleMat2;
  
  QyooMatrix mat = forMat;
  mat.inverse();
  
  // Transform from the big image down into the qyoo small image
  // Set up an image with just the dots, hopefully
  grayImg = new RawImageGray8(sizeX,sizeY);
  grayImg->copyFromGDImage(inImage,mat);
  grayImg->runContrast();
  
  gdImagePtr outImg = grayImg->makeGDImage();
  gdSaveAsPng(outImg,"closeup.png");
  gdImageDestroy(outImg);
}

FeatureDotsProcessor::~FeatureDotsProcessor()
{
	if (grayImg)
		delete grayImg;
	if (gaussImg)
		delete gaussImg;
	if (gradImg)
		delete gradImg;
	feat = NULL;
}

int searchDirX[] = {+1,0,-1,0};
int searchDirY[] = {0,+1,0,-1};

/*
// Decide if the given pixel is a local gradient minima
static bool isInHole(RawImageGray32 *gradImg,int startX,int startY,int dotSize,int jitter)
{
	for (unsigned int ii=0;ii<jitter;ii++)
		for (unsigned int jj=0;jj<jitter;jj++)
		{
			int holeX = startX+ii-jitter/2;
			int holeY = startY+jj-jitter/2;
	
			int numIncrease = 0;
			int holeVal = gradImg->getPixel(holeX, holeY);
			
			// Size of the search in each direction
			int searchSize = dotSize/2;
			
			// Search in each direction, looking for an increase
			for (unsigned int ii=0;ii<4;ii++)
			{
				int dirX = searchDirX[ii];
				int dirY = searchDirY[ii];
				for (unsigned int si=1;si<=searchSize;si++)
				{
					int newGradVal = gradImg->getPixel(holeX+si*dirX, holeY+si*dirY);
					if (newGradVal-holeVal > 100)
					{
						numIncrease++;
						break;
					}
				}
			}
			
			// If there were increases in 3 directions, we're done
			if (numIncrease >= 3)
				return true;
		}
	
	return false;
}
 */

// Calculate an average pixel for the given area
// Note: What I don't like about this is how stray pixels might change this value
static int calcAvgPixel(RawImageGray8 *img,int px,int py,ConvolutionFilterInt *radFilter)
{
	std::vector<int> results(radFilter->getSize()*radFilter->getSize());
	
	radFilter->processPixel(img,px,py,&results[0]);
	
	// Run through and calculate the average
	float val = 0.0;
	for (unsigned int ii=0;ii<radFilter->getSize()*radFilter->getSize();ii++)
		val += results[ii];
	
	return val / radFilter->getFact();
}

// We consider a match to be this close for grayscale radiance (note: not exactly radiance, fine)
const int RadianceDistMatch = 60;
const float PassRatio = .40;  // 40% coverage

// Decide if the given area consists of a dot
// We're looking for a majority of values within the dot to match what we're given
//  within a certain tolerance
static bool isAdot(RawImageGray8 *img,int px,int py,int pixelsInDot,ConvolutionFilterInt *radFilter,int backColor)
{
	std::vector<int> results(radFilter->getSize()*radFilter->getSize());
	radFilter->processPixel(img,px,py,&results[0]);
	
	// Decide if the background color is "blackish" or "whiteish"
	bool isWhite = (backColor >= 128);
	
	// Run through and look for matching pixels
	int numMatch = 0;
	for (unsigned int ii=0;ii<radFilter->getSize()*radFilter->getSize();ii++)
	{
		int thisColor = results[ii];
		if (thisColor >= 0)
		{
			// Distance from this pixel to the grey value we're after
			int dist = backColor - thisColor;  if (dist < 0) dist *= -1;
		
			// The radiance needs to be far enough away and it needs to be
			//  on the opposite side of 128
			if (dist > RadianceDistMatch && ((isWhite && thisColor < 128+32) ||(!isWhite && thisColor > 128-32)))
				numMatch++;
		}
	}
	
	float ratio = (float)numMatch / (float)radFilter->getFact();
	
	return (ratio >= PassRatio);
}

// BEGIN JBB Added
static std::string dec2bin(int intDec)
{
	std::string strBin = "";
	while (intDec) {
		// NSLog(@"%i %i  %i", intDec, (int)intDec / 2, intDec % 2);
//		strBin = [NSString stringWithFormat:@"%i%@", intDec % 2, strBin];
		std::stringstream sstream;
		sstream << (intDec % 2) << strBin;
		strBin = sstream.str();
		intDec = (int)intDec / 2;
	}
	// pad with extra characters
	while (strBin.size() < 6) {
		strBin = (std::string)"0" + strBin;
//		strBin = [NSString stringWithFormat:@"%i%@", 0, strBin];
	}
//	NSLog(@"result: %@", strBin);
	return (strBin);
}

/*
static double bin2dec(const std::string &strBin)
{
	double intDec = 0;
	for (int c = 0; c < strBin.size(); c++) {
//		NSRange range = {c,1};
//		intDec = intDec + ([[strBin substringWithRange:range] intValue] * pow(2, ([strBin length] - c)));
		intDec += ((int)(strBin[c]-'0')) * pow(2,(strBin.size()-c));
	}
	return (intDec);
}
 */
// END JBB Added

// Look for the dots using the grayscale image
void FeatureDotsProcessor::findDotsGray()
{
	QyooModel *qyooModel = QyooModel::getQyooModel();
	ConvolutionFilterInt *radFilter = MakeRadiusFilter(PixelsPerDot,PixelsPerDot/2);
		
	// Reduce noise with the Gauss filter
//	gaussImg = new RawImageGray8(grayImg->getSizeX(),grayImg->getSizeY());
//	featProc.gaussFilter->processImage(grayImg, gaussImg);

	// Look at dot (-1,-1) to get the background color
	int avgPixel = calcAvgPixel(grayImg,PixelsPerDot/2,PixelsPerDot/2,radFilter);

	// Work our way through the dots
	// There's a border of one dot around the whole thing
	int numRow = qyooModel->numRows();
	int numPos = qyooModel->numPos();
	feat->dotStr.clear();
	
	// JBB: Added
	qyooBits = "";
	
	for (unsigned int row=0;row<numRow;row++)
	{
		int resChar = 0;
		int rowPix = PixelsPerDot*(row+1) + PixelsPerDot/2;
		for (unsigned int pos=0;pos<numPos;pos++)
		{
			int posPix = PixelsPerDot*(pos+1) + PixelsPerDot/2;
			
			// If we're in a gradient hole here, there must be a dot
			if (isAdot(grayImg,posPix,rowPix,PixelsPerDot,radFilter,avgPixel))
				resChar |= 1<<pos;
		}
		
		unsigned char theChar;
		
		// JBB Added
		qyooBits = dec2bin(resChar) + qyooBits;
		qyooRows [numRow - row - 1] = resChar;
		
		qyooModel->bitsToChar(resChar,theChar);
		feat->dotBits.push_back(resChar);
		feat->dotStr.push_back(theChar);
		
//		NSLog(@"%d ",(int)resChar);
//		fprintf(stdout,"%d \n",(int)resChar);
	}
		
	for (int n=0; n<6; n++) {
		if (qyooRows[n] < 10) {
			fprintf(stdout, "0%i", qyooRows[n]);
		} else {
			fprintf(stdout, "%i", qyooRows[n]);
		}
	}
	fprintf(stdout, "\n");
	
	std::reverse(feat->dotStr.begin(),feat->dotStr.end());
	
	delete radFilter;
}

FeatureProcessor::FeatureProcessor(gdImagePtr inImage,int sizeX,int sizeY)
{
	grayImg = NULL;
	gaussImg = NULL;
	gradImg = NULL;
	thetaImg = NULL;
	featImg = NULL;
	numFound = 0;
	
	grayImg = new RawImageGray8(sizeX,sizeY);
	grayImg->copyFromGDImage(inImage);
	grayImg->runContrast();
}

FeatureProcessor::~FeatureProcessor()
{
	if (gaussFilter)
		delete gaussFilter;
	if (grayImg)
		delete grayImg;
	if (gaussImg)
		delete gaussImg;
	if (gradImg)
		delete gradImg;
	if (thetaImg)
		delete thetaImg;
	if (featImg)
		delete featImg;
	for (unsigned int ii=0;ii<featureDots.size();ii++)
	  delete featureDots[ii];
}

// Process the image up to the point of finding thin edges
void FeatureProcessor::processImage()
{
	gaussFilter = MakeGaussianFilter_1_4();
	gaussImg = new RawImageGray8(grayImg->getSizeX(),grayImg->getSizeY());

	// Reduce noise with the Gauss filter
	gaussFilter->processImage(grayImg, gaussImg);

	// Calculate intensity gradient and angle of edges
	gradImg = new RawImageGray32(grayImg->getSizeX(),grayImg->getSizeY());

	thetaImg = new RawImageGray8(grayImg->getSizeX(),grayImg->getSizeY());
	CannyGradientAndTheta(gaussImg,gradImg,thetaImg);

	// Now do the non-maximal supression
	CannyNonMaxSupress(gradImg,thetaImg,60.0);
}

// Redo the gauss image at larger size
void FeatureProcessor::redoGradient(gdImagePtr inImage,int sizeX,int sizeY)
{
	if (grayImg)
		delete grayImg;
	grayImg = new RawImageGray8(sizeX,sizeY);
	grayImg->copyFromGDImage(inImage);
	grayImg->runContrast();

	if (gaussImg)
		delete gaussImg;
	gaussImg = new RawImageGray8(grayImg->getSizeX(),grayImg->getSizeY());

	// Reduce noise with the Gauss filter
	gaussFilter->processImage(grayImg, gaussImg);

	// Calculate intensity gradient and angle of edges
	if (gradImg)
		delete gradImg;
	gradImg = new RawImageGray32(grayImg->getSizeX(),grayImg->getSizeY());

	if (thetaImg)
		delete thetaImg;
	thetaImg = new RawImageGray8(grayImg->getSizeX(),grayImg->getSizeY());
	CannyGradientAndTheta(gaussImg,gradImg,thetaImg);

	// Now do the non-maximal supression
	CannyNonMaxSupress(gradImg,thetaImg,60.0);
}

// How close the beginning can be from the end to declare a feature closed
const float ClosedDist = 2.0;
// How big a difference we'll tolerate in decimation
const float DecimateDist = 0.85;

// Find the features in general and then the ones we're after
// Return the number of valid qyoo we found
int FeatureProcessor::findQyoo()
{    
	featImg = new RawImageGray32(grayImg->getSizeX(),grayImg->getSizeY());
	CannyFindFeatures(gradImg, thetaImg, 10.0, 60.0, feats, featImg);

	// Work through the list of features, trying to find a good one
	numFound = 0;
	for (unsigned int ii=0;ii<feats.size();ii++)
	{
	  Feature &feat = feats[ii];
		feat.imgSizeX = grayImg->getSizeX();  feat.imgSizeY = grayImg->getSizeY();
		
		// Decide if it's nominally closed
		feat.calcClosed(ClosedDist*ClosedDist);
		
		// Get rid of unnecessary points within a certain tolerance
		feat.decimate(DecimateDist*DecimateDist);
		
		// Decide if the feature is the wrong shape
		feat.checkSizeAndPosition(grayImg->getSizeX(),grayImg->getSizeY());
		
		// Now look for a corner
		if (feat.valid)
			feat.findCorner();
		
		// Next, use the geometry to find long edges near the corner
		if (feat.valid)
			feat.refineCornerAndFindAngles(10*10);
		
		// Lastly, do a model check
		if (feat.valid)
			feat.modelCheck(0.04*0.04,.8);
		
		if (feat.valid)
			numFound++;
	}
	
	return numFound;	
}

// Find the dots in the valid qyoo
void FeatureProcessor::findDots(gdImagePtr inImage)
{
	// Work through the features for the good ones
	for (unsigned int ii=0;ii<feats.size();ii++)
	{
		Feature &feat = feats[ii];
		if (feat.valid)
		{
			FeatureDotsProcessor *featDots = new 								FeatureDotsProcessor(inImage,this,&feat);
			featDots->findDotsGray();
			featureDots.push_back(featDots);
		}
	}
}
