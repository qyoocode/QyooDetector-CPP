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
#include <sstream>

#import "FeatureDetector.h"
#import "QyooModel.h"
#include "Logger.h"

// Pixels per dot for detection
const int PixelsPerDot = 11;

// Define constants for closing distance and decimation tolerance
const float ClosedDist = 2.0f;  // Threshold distance to consider a feature closed
const float DecimateDist = 0.85f;  // Tolerance for decimating points in a feature

// FeatureDotsProcessor constructor
// Initializes the dot processor with an image, a feature processor, and a feature.
FeatureDotsProcessor::FeatureDotsProcessor(gdImagePtr inImage, FeatureProcessor *inFeatProc, Feature *inFeat)
{
    init(inImage, inFeatProc, inFeat);
}

// Initialize the dot processor
void FeatureDotsProcessor::init(gdImagePtr inImage, FeatureProcessor *inFeatProc, Feature *inFeat)
{
    grayImg = nullptr;
    gaussImg = nullptr;
    gradImg = nullptr;
    featProc = inFeatProc;
    feat = inFeat;

    QyooModel *qyooModel = QyooModel::getQyooModel();
    float imgWidth = gdImageSX(inImage);
    float imgHeight = gdImageSY(inImage);

    // Determine the size of the image to render to
    int sizeX = PixelsPerDot * (qyooModel->numRows() + 2);
    int sizeY = PixelsPerDot * (qyooModel->numPos() + 2);

    // Apply transformations to convert the image to Qyoo space
    float scaleX = imgWidth / feat->imgSizeX;
    float scaleY = imgHeight / feat->imgSizeY;
    QyooMatrix scaleMat(scaleX, 0.0, 0.0, 0.0, scaleY, 0.0, 0.0, 0.0, 1.0);
    QyooMatrix forMat = scaleMat * feat->mat;

    // Add a margin of space around the dots
    SimplePoint2D ll, ur;
    qyooModel->dotBounds(ll, ur, true);
    QyooMatrix transMat(1.0, 0.0, ll.x, 0.0, 1.0, ll.y, 0.0, 0.0, 1.0);
    QyooMatrix scaleMat2(ur.x - ll.x, 0.0, 0.0, 0.0, ur.y - ll.y, 0.0, 0.0, 0.0, 1.0);
    forMat = forMat * transMat * scaleMat2;

    // Inverse the transformation matrix to map back to Qyoo space
    QyooMatrix mat = forMat;
    mat.inverse();

    // Convert the image to grayscale and apply contrast
    grayImg = new RawImageGray8(sizeX, sizeY);
    grayImg->copyFromGDImage(inImage, mat);
    grayImg->runContrast();
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

    float val = 0.0;
    for (int result : results)
        val += result;

    return val / radFilter->getFact();
}

// Constants used for dot detection
const int RadianceDistMatch = 60;
const float PassRatio = .40;  // 40% coverage

// Check if an area contains a dot by comparing the radiance of pixels
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
    QyooModel *qyooModel = QyooModel::getQyooModel();
    ConvolutionFilterInt *radFilter = MakeRadiusFilter(PixelsPerDot, PixelsPerDot / 2);

    int avgPixel = calcAvgPixel(grayImg, PixelsPerDot / 2, PixelsPerDot / 2, radFilter);

    int numRow = qyooModel->numRows();
    int numPos = qyooModel->numPos();
    feat->dotBinStr.clear();
    feat->dotDecStr.clear();
    qyooBits = "";  // Start fresh with qyooBits

    for (int row = 0; row < numRow; row++) {  // We process from row 0 to numRow
        int resChar = 0;
        int rowPix = PixelsPerDot * (row + 1) + PixelsPerDot / 2;

        for (unsigned int pos = 0; pos < numPos; pos++) {
            int posPix = PixelsPerDot * (pos + 1) + PixelsPerDot / 2;

            if (isAdot(grayImg, posPix, rowPix, PixelsPerDot, radFilter, avgPixel)) {
                resChar |= 1 << pos;
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

        logVerbose("Row " + std::to_string(row) + ": resChar (binary) = " + currentRowBits);
        logVerbose("Current qyooBits = " + qyooBits);
    }
    feat->dotBinStr = qyooBits;

    // Convert the full binary string (qyooBits) to decimal, but first ensure it's within the range of an unsigned long long
    if (qyooBits.size() > 64) {
        std::cerr << "Error: qyooBits exceeds 64 bits, cannot convert to unsigned long long." << std::endl;
    } else {
        std::cout << "Binary = " << qyooBits << std::endl;

        feat->dotDecStr = std::to_string(std::stoull(qyooBits, nullptr, 2));  // Convert binary string to decimal
        std::cout << "Qyoo value = " << feat->dotDecStr << std::endl;
    }

    delete radFilter;

}


// FeatureProcessor constructor: initialize with an image
FeatureProcessor::FeatureProcessor(gdImagePtr inImage, int sizeX, int sizeY)
{
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
    logVerbose("Starting Qyoo detection...");

    featImg = new RawImageGray32(grayImg->getSizeX(), grayImg->getSizeY());
    CannyFindFeatures(gradImg, thetaImg, 10.0, 60.0, feats, featImg);

    logVerbose("Number of features detected: " + std::to_string(feats.size()) );

    // Iterate over the detected features and validate them
    numFound = 0;
    for (unsigned int ii = 0; ii < feats.size(); ii++)
    {
        Feature &feat = feats[ii];

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
            logVerbose("Qyoo shape feature found!");
            numFound++;
        }
    }

    logVerbose("Total Qyoo shapes detected: " + std::to_string(numFound) );
    return numFound;
}

// Detect dots in the valid Qyoo features
void FeatureProcessor::findDots(gdImagePtr inImage)
{
    for (auto &feat : feats)
    {
        if (feat.valid)
        {
            auto *featDots = new FeatureDotsProcessor(inImage, this, &feat);
            featDots->findDotsGray();
            featureDots.push_back(featDots);
        }
    }
}
