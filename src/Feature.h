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

/* Vector feature
	Initially represents a list of points and, as it passes various tests,
    gains more qyoo specific information.
 */
class Feature
{
public:
	Feature() { valid = true; cornerValid = false;  edgesValid = false;  farEdgesValid = false; closed = false;};
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
				
	// Single point structure.  Yeah, should be elsewhere.
	class Point
	{
	public:
		Point() { };
		Point(int inX,int inY) { x = inX;  y = inY; }
		int x,y;
	};
	bool valid;        // Overally validity of the feature
	
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
	
	// The raw bits for the dots, if they've been read
	std::vector<unsigned char> dotBits;
	
	// If the dot reader has been run, this is the resulting string
	std::string dotStr;

	std::list<Point> origPoints;  // After decimation, keep the original points around
	std::list<Point> points;
	
protected:
	// Internal utility routine for finding the farthest point from a line
	bool findFarPoint(float p0x,float p0y,float p1x,float p1y,Point &far,float &retDist);
	
	// See if just this point is close to the model version
	bool pointModelCheck(QyooMatrix *invTrans,float x,float y,float nearDist2);
    
};

#ifndef QYOO_CMD

// A whole group of features
// Mostly just an Objective C wrapper for display
@interface FeatureSet : NSObject
{
@public
	int sizeX,sizeY; // Overall size of image
	std::vector<Feature> feats;
}
@end

#endif

