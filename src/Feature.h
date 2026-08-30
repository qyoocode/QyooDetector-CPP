/*
 *  Feature.h
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/20/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#import <vector>
#import <list>
#import <string>
#import "Geometry.h"
#import "RawImage.h"

class DetectorLogger;

enum FeatureRejectionReason
{
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

const char *featureRejectionReasonCode(FeatureRejectionReason reason);

/* Vector feature
	Initially represents a list of points and, as it passes various tests,
    gains more qyoo specific information.
 */
class Feature
{
public:
	Feature()
	{
		valid = true;
		cornerValid = false;
		edgesValid = false;
		farEdgesValid = false;
		closed = false;
		modelChecked = false;
		projectiveValid = false;
		carrierProjectiveValid = false;
		carrierCirclePointCount = 0;
		carrierFirstEdgePointCount = 0;
		carrierSecondEdgePointCount = 0;
		carrierProjectiveRmsError = 0.0;
		carrierProjectiveMaxError = 0.0;
		projectiveIterations = 0;
		projectiveCorrespondenceCount = 0;
		projectiveRmsError = 0.0;
		projectiveMaxError = 0.0;
		projectiveDotRefined = false;
		projectiveDotCorrespondenceCount = 0;
		projectiveDotRmsError = 0.0;
		projectiveRefinedOutlineRmsError = 0.0;
		projectiveRefinedOutlineMaxError = 0.0;
		projectiveRefinedOutlineNearFraction = 0.0;
		rejectionReason = FeatureNotRejected;
		boundsMinX = boundsMinY = boundsMaxX = boundsMaxY = 0;
		boundsWidth = boundsHeight = 0;
		aspectRatio = areaFraction = 0.0;
		originalPointCount = decimatedPointCount = 0;
		nearCornerEdgeCount = 0;
		cornerAngleDifference = 0.0;
		modelClosePointCount = modelTotalPointCount = 0;
		modelCloseFraction = 0.0;
		sizeCheckPassed = false;
		cornerGeometryPassed = false;
		outlineCheckPassed = false;
		affineNormalizationAttempted = false;
		affineNormalizationAvailable = false;
		affinePayloadExtracted = false;
		projectiveNormalizationAttempted = false;
		projectiveNormalizationAvailable = false;
		projectiveAffineFallbackUsed = false;
		projectiveAffineFallbackRejected = false;
			projectivePayloadExtracted = false;
			logger = nullptr;
	};
	Feature(const Feature &that) { *this = that; }
	~Feature() { };
    
	// Tack a point on the end
	void addPointEnd(int cx,int cy);
	// Stick a point at the beginning
	void addPointBegin(int cx,int cy);
	
	// Simply the outline within the given distance^2
	void decimate(float dist2);
	
	// Check if it's closed (within a certain tolerance)
	void calcClosed(int dist2);
	
	// Check the overall size, position and aspect ratio
	// Reject this feature if it's too small
	void checkSizeAndPosition(int imgSizeX,int imgSizeY);
	
	// Try to find the corner
	void findCorner();
	
	// Use nearby geometry to refine the corner position
	//  or just reject it.
	// Also records directions (just points) for the edges
	// And finds the far edge, if it's there
	void refineCornerAndFindAngles(int searchDist2);
	
	// Check the actual points against where we think they ought to be
	bool modelCheck(float nearDist2,float nearFrac);

	// Fit the accepted outline to the existing Qyoo model without changing
	// candidate acceptance. The historical affine transform is the initializer.
	bool estimateProjectiveTransform(int iterations = 12);
	// Recover the seven observable projective degrees of freedom from the two
	// straight carrier edges and the fitted curved boundary. The remaining
	// one-dimensional ambiguity is represented explicitly by
	// QyooModel::carrierAmbiguityTransform().
	bool estimateCarrierProjectiveClass();
	bool affineFallbackGeometryQualified(double maximumAngleDeviationDegrees) const;
	bool projectiveOutlineError(const ProjectiveTransform &transform,
	                           double &rmsError, double &maxError) const;
	double projectiveOutlineNearFraction(const ProjectiveTransform &transform,
	                                    double nearDistance) const;
				
	// Single point structure.  Yeah, should be elsewhere.
	class Point
	{
	public:
		Point() { };
		Point(int inX,int inY) { x = inX;  y = inY; }
		int x,y;
	};
	bool valid;        // Overally validity of the feature
	DetectorLogger *logger; // Non-owning, context-local diagnostic logger
	FeatureRejectionReason rejectionReason;
	int boundsMinX,boundsMinY,boundsMaxX,boundsMaxY;
	int boundsWidth,boundsHeight;
	double aspectRatio,areaFraction;
	size_t originalPointCount,decimatedPointCount;
	int nearCornerEdgeCount;
	std::vector<double> nearCornerEdgeLengths;
	std::vector<double> nearCornerEdgeAngles;
	double cornerAngleDifference;
	int modelClosePointCount,modelTotalPointCount;
	double modelCloseFraction;
	bool sizeCheckPassed;
	bool cornerGeometryPassed;
	bool outlineCheckPassed;
	
	int imgSizeX,imgSizeY;  // Size of image we found the feature in

	bool closed;       // We've decided it's closed

	bool cornerValid;  // We found a corner
	float cornX,cornY;
	
	bool edgesValid;   // Set if the corner edges are set
	Point e0,e1;       // Edges from the corner point.  Defines the corner.
	float ang0,ang1;   // Angles of the two edges

	bool farEdgesValid;     // Set if we found some good far edges
	Point far0,far1;
	float dist0,dist1;      // Square of distance from far points to their edges
	float sheer;            // A shear value to move the model to e1
	
	bool modelChecked;      // Passed model check

	// Transformation from Qyoo model space to image space
	QyooMatrix mat;
	ProjectiveTransform projectiveMat;
	ProjectiveTransform carrierProjectiveMat;
	bool projectiveValid;
	bool carrierProjectiveValid;
	int carrierCirclePointCount;
	int carrierFirstEdgePointCount;
	int carrierSecondEdgePointCount;
	double carrierProjectiveRmsError;
	double carrierProjectiveMaxError;
	int projectiveIterations;
	int projectiveCorrespondenceCount;
	double projectiveRmsError;
	double projectiveMaxError;
	bool projectiveDotRefined;
	int projectiveDotCorrespondenceCount;
	double projectiveDotRmsError;
	double projectiveRefinedOutlineRmsError;
	double projectiveRefinedOutlineMaxError;
	double projectiveRefinedOutlineNearFraction;
	// Decision-neutral copies of the interior correspondences already used by
	// projective refinement. They are exposed only by visual diagnostics.
	std::vector<ProjectivePoint> projectiveDotModelPoints;
	std::vector<ProjectivePoint> projectiveDotImagePoints;
	bool affineNormalizationAttempted;
	bool affineNormalizationAvailable;
	bool affinePayloadExtracted;
	bool projectiveNormalizationAttempted;
	bool projectiveNormalizationAvailable;
	bool projectiveAffineFallbackUsed;
	bool projectiveAffineFallbackRejected;
	bool projectivePayloadExtracted;
	
	// The raw bits for the dots, if they've been read
	std::vector<unsigned char> dotBits;
	
	// If the dot reader has been run, these are the resulting stringa
	std::string dotBinStr;
	std::string dotDecStr;

	std::list<Point> origPoints;  // After decimation, keep the original points around
	std::list<Point> points;
	
protected:
	// Internal utility routine for finding the farthest point from a line
	bool findFarPoint(float p0x,float p0y,float p1x,float p1y,Point &far,float &retDist);
	
	// See if just this point is close to the model version
	bool pointModelCheck(QyooMatrix *invTrans,float x,float y,float nearDist2);
    
};
