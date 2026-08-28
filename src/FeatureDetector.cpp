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

// FeatureDotsProcessor constructor
// Initializes the dot processor with an image, a feature processor, and a feature.
FeatureDotsProcessor::FeatureDotsProcessor(gdImagePtr inImage, FeatureProcessor *inFeatProc,
                                           Feature *inFeat, NormalizationMode inNormalization,
                                           const std::string &inResultLabel,
                                           ProjectiveFallbackPolicy inFallbackPolicy)
{
    init(inImage, inFeatProc, inFeat, inNormalization, inResultLabel, inFallbackPolicy);
}

// Initialize the dot processor
void FeatureDotsProcessor::init(gdImagePtr inImage, FeatureProcessor *inFeatProc, Feature *inFeat,
                                NormalizationMode inNormalization, const std::string &inResultLabel,
                                ProjectiveFallbackPolicy inFallbackPolicy)
{
    grayImg = nullptr;
    gaussImg = nullptr;
    gradImg = nullptr;
    featProc = inFeatProc;
    feat = inFeat;
    normalization = inNormalization;
    resultLabel = inResultLabel;
    fallbackPolicy = inFallbackPolicy;
    normalizationAvailable = false;
	if (normalization == NormalizationAffine)
		feat->affineNormalizationAttempted = true;
	else if (normalization == NormalizationProjective)
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
    if (normalization == NormalizationProjective)
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
        RawImageGray8 affinePilot(sizeX, sizeY);
        if (!affinePilot.copyFromGDImageProjective(inImage, affineNormalizedToImage))
            return;
        affinePilot.runContrast();
        if (refineProjectiveFromVisibleDots(&affinePilot, affineNormalizedToImage, feat))
        {
            logVerbose("Projective dot refinement: " +
                       std::to_string(feat->projectiveDotCorrespondenceCount) +
                       " centers, RMS " + std::to_string(feat->projectiveDotRmsError));
            logVerbose("Projective dot matrix: [" +
                       std::to_string(feat->projectiveMat.at(0, 0)) + "," +
                       std::to_string(feat->projectiveMat.at(0, 1)) + "," +
                       std::to_string(feat->projectiveMat.at(0, 2)) + ";" +
                       std::to_string(feat->projectiveMat.at(1, 0)) + "," +
                       std::to_string(feat->projectiveMat.at(1, 1)) + "," +
                       std::to_string(feat->projectiveMat.at(1, 2)) + ";" +
                       std::to_string(feat->projectiveMat.at(2, 0)) + "," +
                       std::to_string(feat->projectiveMat.at(2, 1)) + "," +
                       std::to_string(feat->projectiveMat.at(2, 2)) + "]");
            logVerbose("Projective refined-outline fit: RMS " +
                       std::to_string(feat->projectiveRefinedOutlineRmsError) +
                       ", max " + std::to_string(feat->projectiveRefinedOutlineMaxError) +
                       ", near fraction " +
                       std::to_string(feat->projectiveRefinedOutlineNearFraction));
        }
        else
        {
            if (feat->projectiveDotCorrespondenceCount >= 4)
            {
                logVerbose("Projective rejected dot refinement: " +
                           std::to_string(feat->projectiveDotCorrespondenceCount) +
                           " centers, dot RMS " +
                           std::to_string(feat->projectiveDotRmsError) +
                           ", outline RMS " +
                           std::to_string(feat->projectiveRefinedOutlineRmsError) +
                           ", outline max " +
                           std::to_string(feat->projectiveRefinedOutlineMaxError) +
                           ", near fraction " +
                           std::to_string(feat->projectiveRefinedOutlineNearFraction));
                logVerbose("Projective dot refinement rejected by accepted-outline validation; payload not sampled.");
                feat->projectiveValid = false;
                return;
            }
            bool allowFallback = fallbackPolicy == LegacyAffineFallback ||
                (fallbackPolicy == QualifiedAffineFallback &&
                 feat->affineFallbackGeometryQualified(MaximumQualifiedAffineAngleDeviationDegrees));
            if (!allowFallback)
            {
                feat->projectiveAffineFallbackRejected = true;
                logVerbose("Projective dot refinement unavailable; affine fallback rejected by policy.");
                feat->projectiveValid = false;
                return;
            }
            logVerbose("Projective dot refinement unavailable; using affine fallback.");
			feat->projectiveAffineFallbackUsed = true;
        }
        ProjectiveTransform normalizedToImage = feat->projectiveDotRefined
            ? inputScale * feat->projectiveMat * dotTranslation * dotScale
            : affineNormalizedToImage;
        if (!grayImg->copyFromGDImageProjective(inImage, normalizedToImage))
            return;
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
    }
    grayImg->runContrast();
    normalizationAvailable = true;
	if (normalization == NormalizationAffine)
		feat->affineNormalizationAvailable = true;
	else if (normalization == NormalizationProjective)
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
    if (!normalizationAvailable)
    {
        logVerbose((resultLabel.empty() ? std::string("Projective") : resultLabel) +
                   " normalization unavailable; payload not sampled.");
        return;
    }
    QyooModel *qyooModel = QyooModel::getQyooModel();
    ConvolutionFilterInt *radFilter = MakeRadiusFilter(PixelsPerDot, PixelsPerDot / 2);

    int avgPixel = calcAvgPixel(grayImg, PixelsPerDot / 2, PixelsPerDot / 2, radFilter);

    int numRow = qyooModel->numRows();
    int numPos = qyooModel->numPos();
    feat->dotBinStr.clear();
    feat->dotDecStr.clear();
    qyooBits = "";  // Start fresh with qyooBits

    // Create an RGB image to draw on
    gdImagePtr outImg = gdImageCreateTrueColor(grayImg->getSizeX(), grayImg->getSizeY());
    gdImagePtr grayVisualization = grayImg->makeGDImage();

    // Copy the grayscale data into the RGB image (mapping grayscale values to RGB)
    for (int x = 0; x < grayImg->getSizeX(); x++) {
        for (int y = 0; y < grayImg->getSizeY(); y++) {
            int grayValue = gdImageGetPixel(grayVisualization, x, y);
            int rgbColor = gdImageColorAllocate(outImg, grayValue, grayValue, grayValue);
            gdImageSetPixel(outImg, x, y, rgbColor);
        }
    }
    gdImageDestroy(grayVisualization);

    // Allocate colors for drawing
    int colorRed = gdImageColorAllocate(outImg, 255, 0, 0);
    int colorGreen = gdImageColorAllocate(outImg, 0, 255, 0);

    for (int row = 0; row < numRow; row++) {  // We process from row 0 to numRow
        int resChar = 0;
        int rowPix = PixelsPerDot * (row + 1) + PixelsPerDot / 2;

        for (unsigned int pos = 0; pos < numPos; pos++) {
            int posPix = PixelsPerDot * (pos + 1) + PixelsPerDot / 2;

            if (isAdot(grayImg, posPix, rowPix, PixelsPerDot, radFilter, avgPixel)) {
                resChar |= 1 << pos;

                // Draw a green circle around the detected dot
                gdImageArc(outImg, posPix, rowPix, PixelsPerDot, PixelsPerDot, 0, 360, colorGreen);
            } else {
                // If the dot represents a 0, draw a red X
                gdImageLine(outImg, posPix - 5, rowPix - 5, posPix + 5, rowPix + 5, colorRed);    // Draw slash left
                gdImageLine(outImg, posPix - 5, rowPix + 5, posPix + 5, rowPix - 5, colorRed);    // Draw slash right
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
        std::string prefix = resultLabel.empty() ? "" : resultLabel + " ";
        std::cout << prefix << "Binary = " << qyooBits << std::endl;

        feat->dotDecStr = std::to_string(std::stoull(qyooBits, nullptr, 2));  // Convert binary string to decimal
        std::cout << prefix << "Qyoo value = " << feat->dotDecStr << std::endl;
		if (normalization == NormalizationAffine)
			feat->affinePayloadExtracted = true;
		else if (normalization == NormalizationProjective)
			feat->projectivePayloadExtracted = true;
    }

    // Save the image with the dots circled and x notated to see where pattern is detected
    std::string outputFilePrefix = resultLabel.empty() ? "" : resultLabel + "_";
    std::string outputFileName = "output/" + outputFilePrefix + feat->dotDecStr + ".png";
    FILE *outputFile = fopen(outputFileName.c_str(), "wb");
    if (outputFile) {
        gdImagePng(outImg, outputFile); // Save PNG image using gdImagePng
        fclose(outputFile);
    } else {
        std::cerr << "Error: Unable to open file for writing PNG image: " << outputFileName << std::endl;
    }

    gdImageDestroy(outImg);

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
			feat.estimateProjectiveTransform();
			if (feat.projectiveValid)
			{
				logVerbose("Projective outline fit: " + std::to_string(feat.projectiveCorrespondenceCount) +
				           " points, RMS " + std::to_string(feat.projectiveRmsError) +
				           ", max " + std::to_string(feat.projectiveMaxError));
			}
			else
			{
				logVerbose("Projective outline fit unavailable.");
			}
            numFound++;
        }
    }

    logVerbose("Total Qyoo shapes detected: " + std::to_string(numFound) );
    return numFound;
}

// Detect dots in the valid Qyoo features
void FeatureProcessor::findDots(gdImagePtr inImage, NormalizationMode normalization,
                                ProjectiveFallbackPolicy fallbackPolicy)
{
    for (auto &feat : feats)
    {
        if (feat.valid)
        {
            if (normalization == NormalizationShadow)
            {
                auto *affineDots = new FeatureDotsProcessor(inImage, this, &feat,
                                                            NormalizationAffine, "", fallbackPolicy);
                affineDots->findDotsGray();
                featureDots.push_back(affineDots);
                auto *projectiveDots = new FeatureDotsProcessor(inImage, this, &feat,
                                                                NormalizationProjective, "Projective", fallbackPolicy);
                projectiveDots->findDotsGray();
                featureDots.push_back(projectiveDots);
            }
            else
            {
                auto *featDots = new FeatureDotsProcessor(inImage, this, &feat, normalization, "", fallbackPolicy);
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
        case NormalizationShadow: return "shadow";
    }
    return "unknown";
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

static const char *normalizationOutcomeCode(const Feature &feature,
                                            NormalizationMode normalization)
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

std::string FeatureProcessor::diagnosticsJson(const std::string &imageId,
                                              NormalizationMode normalization,
                                              ProjectiveFallbackPolicy fallbackPolicy) const
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
        output << ",\"projective_dot_correspondence_count\":" << feature.projectiveDotCorrespondenceCount;
        output << ",\"projective_dot_refined\":" << jsonBoolean(feature.projectiveDotRefined);
        output << ",\"projective_affine_fallback_used\":" << jsonBoolean(feature.projectiveAffineFallbackUsed);
        output << ",\"projective_affine_fallback_rejected\":" << jsonBoolean(feature.projectiveAffineFallbackRejected);
        output << ",\"affine_normalization_attempted\":" << jsonBoolean(feature.affineNormalizationAttempted);
        output << ",\"projective_normalization_attempted\":" << jsonBoolean(feature.projectiveNormalizationAttempted);
        output << ",\"normalization_available\":"
               << jsonBoolean(feature.affineNormalizationAvailable || feature.projectiveNormalizationAvailable);
        output << ",\"payload_extracted\":"
               << jsonBoolean(feature.affinePayloadExtracted || feature.projectivePayloadExtracted);
        output << ",\"normalization_outcome\":"
               << jsonString(normalizationOutcomeCode(feature, normalization));
        output << '}';
    }
    output << "]}";
    return output.str();
}
