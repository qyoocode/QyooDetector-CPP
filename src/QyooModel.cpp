/*
 *  QyooModel.mm
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/30/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#include <iostream>
#include <sstream>
#include <bitset>
#include "QyooModel.h"

// Instantiate the singleton
QyooModel *QyooModel::getQyooModel()
{
	static QyooModel *theModel = NULL;
	
	if (!theModel)
		theModel = new QyooModel();
	
	return theModel;
}

// Set up the model according to the dots and the
//  internal buffer
QyooModel::QyooModel(int numDotX,int numDotY,float buffer)
{
	this->numDotX = numDotX;
	this->numDotY = numDotY;
	this->buffer = buffer;
	
	// Calculate two points on the internal square
	float rad = 0.5-2*buffer;  // Don't make the buffer too big
	SimplePoint2D org(0.5,0.5);
	SimplePoint2D dir(-0.5,-0.5);  dir.norm();
	ll = dir * rad + org;  ur = dir * -rad + org;

	// Precalculate the dot radius
	int numDots = (numDotX > numDotY ? numDotX : numDotY);
	
	dotRad = (ur.x-ll.x)/(2.0*numDots);
	
	addLegacy(00,30,30,30,30,00); // qyoo logo
}

// Add a legacy code to our list
void QyooModel::addLegacy(unsigned char q5,unsigned char q4,unsigned char q3,unsigned char q2,unsigned char q1,unsigned char q0)
{
	long long code = 
		((long long)q0) +
		(((long long)q1) << 8) +
		(((long long)q2) << 16) +
		(((long long)q3) << 24) +
		(((long long)q4) << 32) +
		(((long long)q5) << 40);
	
	legacyQyoos.insert(code);
}

// Return the location of a given dot
SimplePoint2D QyooModel::dotLocation(int row,int pos)
{
	float rad = dotRadius();
	
	SimplePoint2D loc;
	loc.x = row * (2*rad) + ll.x + rad;
	loc.y = pos * (2*rad) + ll.y + rad;
	
	return loc;
}

// Return the radius of a single dot
float QyooModel::dotRadius()
{
	return dotRad;
}

// Calculate the bounding box (within qyoo space) of just the dots
void QyooModel::dotBounds(SimplePoint2D &ll,SimplePoint2D &ur,bool withBorder)
{
	ll = this->ll;
	ur = this->ur;
	
	if (withBorder)
	{
		ll.x -= 2*dotRad;		ll.y -= 2*dotRad;
		ur.x += 2*dotRad;		ur.y += 2*dotRad;
	}
}

// Data for decoding dots
const int MaxEncode = 36;
const char *encodeData = "0123456789abcdefghijklmnopqrstuvwxyz";

// Translate from bit values to a character
bool QyooModel::bitsToChar(int theBits,unsigned char &retChar)
{
	if (theBits < 0 || theBits >= MaxEncode)
	{
		retChar = '0';
		return false;
	}
	
	retChar = encodeData[theBits];
	
	return true;
}

// Convert the bit vector to decimal
unsigned long long QyooModel::decimalCode(const std::vector<unsigned char> &bitVec)
{
	unsigned long long code = 0;
	unsigned long long factor = 1;
	for (unsigned int ii=0;ii<bitVec.size();ii++)
	{
		code += bitVec[ii] * factor;
		factor *= (1<<8);
	}
	
	return code;
}

// Convert a bits vector to a printable version of the code
std::string QyooModel::decimalCodeStr(const std::vector<unsigned char> &bitVec)
{
	std::string theStr;
	
	for (int ii=bitVec.size()-1;ii>=0;ii--)
	{
		std::stringstream strStrm;
		strStrm << (int)bitVec[ii];
		if (strStrm.str().length() < 2)
			theStr += "0";
		theStr += strStrm.str();
	}	
	
	return theStr;
}

bool QyooModel::verifyCode(const std::vector<unsigned char> &bitVec)
{
/*
    // Convert the bit vector to a decimal string
    std::string decStr = decimalCodeStr(bitVec);

    // Display the detected decimal string code
    std::cout << "The detected code (decimal string): " << decStr << std::endl;

    // Display the detected bit pattern without reversing bits
    std::cout << "The detected code (bit pattern): ";
    for (unsigned char byte : bitVec) {
        std::bitset<8> bits(byte);  // No bit reversal
        std::cout << bits << " ";
    }
    std::cout << std::endl;

    // Check if it's a legacy code
    if (legacyQyoos.find(std::stoull(decStr)) != legacyQyoos.end())
        return true;
    */

    // If not found, return false
    return false;
}
