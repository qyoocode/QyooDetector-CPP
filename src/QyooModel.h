/*
 *  QyooModel.h
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/30/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#import <vector>
#import <set>
#import <string>
#import "Geometry.h"

/* This encapsulates the math for a qyoo model.
	For those parts that can be configured, you configure
    them here.  This consists of things like number of dots,
    internal spacing, that sort of thing.

    There are plenty of assumptions about the overall shape
    scattered around the code.  You can't change those here.

	Also note that this model is rotated +90 degrees from
	the qyoo specification.
 */
class QyooModel
{
public:
	// This is a singleton
	static QyooModel *getQyooModel();

	// Return the center of a given dot in model space
	SimplePoint2D dotLocation(int row,int pos);

	// Return the radius of a dot in model space
	float dotRadius();

	// Calculate the extents of the dots
	// If there's a border, add in one more row of dots
	void dotBounds(SimplePoint2D &ll,SimplePoint2D &ur,bool withBorder=false);

	// Translate the given set of bits into a character
	// Returns false if it's out of range
	bool bitsToChar(int theBits,unsigned char &retChar);

	// Convert the bit vector to a decimal version of the code
	// This is shifting by 8, not multiplying by 100
	unsigned long long decimalCode(const std::vector<unsigned char> &bitVec);

	// Convert the bit vector to a string decimal version of the code
	// This is what we'd expect to see displayed on the web page
	std::string decimalCodeStr(const std::vector<unsigned char> &bitVec);

	// Verify that this is a valid code
	bool verifyCode(const std::vector<unsigned char> &bitVec);

	int numRows() { return numDotX; }
	int numPos() { return numDotY; }

//public:
	/* Construct with the number of internal dots and
	 any buffer we put around them internally.
	 By default you've got 6x6 dots and no internal buffer.
	 */
	QyooModel(int numDotX=6,int numDotY=6,float intBuffer=0.0);

	// Toss a legacy qyoo into the mix
	void addLegacy(unsigned char q5,unsigned char q4,unsigned char q3,unsigned char q2,unsigned char q1,unsigned char q0);

	int numDotX,numDotY;
	float buffer;
	SimplePoint2D ll,ur;  // Lower left and upper right of internal square
	float dotRad;         // Radius of a single dot
	// These are valid qyoo codes that don't adhere to the newer error checking
	std::set<unsigned long long> legacyQyoos;
};