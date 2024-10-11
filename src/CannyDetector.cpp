/*
 *  CannyDectector.mm
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/15/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#include "CannyDetector.h"
#include "Logger.h"

// Calculate the gradient magnitude and direction at each pixel
void CannyGradientAndTheta(RawImageGray8 *gaussImg,RawImageGray32 *gradImg,RawImageGray8 *thetaImg)
{
	// Set these up temporarily for the sobel operators
	ConvolutionFilterInt *sobelX = MakeSobelFilterX();
	ConvolutionFilterInt *sobelY = MakeSobelFilterY();
	RawImageGray32 sobelXimg(gaussImg->getSizeX(),gaussImg->getSizeY());
	RawImageGray32 sobelYimg(gaussImg->getSizeX(),gaussImg->getSizeY());
	sobelX->processImage(gaussImg, &sobelXimg);
	sobelY->processImage(gaussImg, &sobelYimg);
	
	int offset = sobelX->getSize()/2;
	for (unsigned int iy=offset;iy<gaussImg->getSizeY()-offset;iy++)
		for (unsigned int ix=offset;ix<gaussImg->getSizeX()-offset;ix++)
		{
			// Calculate the gradient magnitude
			int gx = sobelXimg.getPixel(ix,iy), gy = sobelYimg.getPixel(ix,iy);
			// Approximation of magnitude
			int g = (gx > 0 ? gx : -gx) + (gy > 0 ? gy : -gy);
			gradImg->getPixel(ix,iy) = g;
			
			if (thetaImg != NULL)
			{
				unsigned char thetaPix = ThetaEmpty;
				if (g > 0)
				{
					// Do the direction, consolidated in four directions
					float theta = properAtan(gx,gy);
					if (theta > 180.0)  theta -= 180.0;
					
					if (theta >= 0.0)
					{
						if (theta < 22.5)
							thetaPix = Theta0;
						else
						{
							if (theta < 67.5)
								thetaPix = Theta45;
							else
							{
								if (theta < 112.5)
									thetaPix = Theta90;
								else
								{
									if (theta < 157.5)
										thetaPix = Theta135;
									else
										thetaPix = Theta0;
								}
							}
						}
					}
					
				}
				thetaImg->getPixel(ix, iy) = thetaPix;
			}
		}
	
	delete sobelX;
	delete sobelY;
}

// Run the non-maximal supression
void CannyNonMaxSupress(RawImageGray32 *gradImg,RawImageGray8 *thetaImg,int gradThresh)
{
	int numZero = 0,numNonZero=0;

	// Work at least one pixel in
	for (unsigned int iy=1;iy<gradImg->getSizeY()-1;iy++)
		for (unsigned int ix=1;ix<gradImg->getSizeX()-1;ix++)
		{
			int &g = gradImg->getPixel(ix, iy);
			unsigned char &theta = thetaImg->getPixel(ix, iy);
			
			if (g < gradThresh)
				theta = ThetaEmpty;
			else {
				switch (theta)
				{
					case Theta0:
						if (g > gradImg->getPixel(ix+1, iy) && g > gradImg->getPixel(ix-1, iy))
							theta |= CannyThinFlag;
						break;
					case Theta45:
						if (g > gradImg->getPixel(ix+1, iy+1) && g > gradImg->getPixel(ix-1, iy-1))
							theta |= CannyThinFlag;
						break;
					case Theta90:
						if (g > gradImg->getPixel(ix, iy+1) && g > gradImg->getPixel(ix, iy-1))
							theta |= CannyThinFlag;
						break;
					case Theta135:
						if (g > gradImg->getPixel(ix-1, iy+1) && g > gradImg->getPixel(ix+1, iy-1))
							theta |= CannyThinFlag;
						break;
				}
			}
			
			if (theta && (~CannyThinFlag) == ThetaEmpty)
				numZero++;
			else
				numNonZero++;
		}
	
}

// Calculate the next direction based on the direction we went last time
// Ignore the gradient
void calcNextGridDir(int offset,int cx,int cy,int &nx,int &ny,int &gridDir)
{
	int cur = gridDir + offset;
	if (cur < 0)  cur += 8;
	if (cur >= 8)  cur -= 8;

	int ox = 0;
	int oy = 0;
	switch (cur)
	{
		case 0:
			ox = +1;  oy = 0;
			break;
		case 1:
			ox = +1;  oy = +1;
			break;
		case 2:
			ox = 0;  oy = +1;
			break;
		case 3:
			ox = -1;  oy = +1;
			break;
		case 4:
			ox = -1;  oy = 0;
			break;
		case 5:
			ox = -1;  oy = -1;
			break;
		case 6:
			ox = 0;  oy = -1;
			break;
		case 7:
			ox = +1;  oy = -1;
			break;
	}
	
	gridDir = cur;
	nx = cx + ox;
	ny = cy + oy;
}

// Calculate the next direction based on the current gradient dir
// And an offset, which is sort of like an angle broken into 8 pieces
void calcNextThetaDir(ThetaAngles dir,int offset,int cx,int cy,int &nx,int &ny,int &gridDir)
{
	// If there is a preferred grid dir, just use that
	if (gridDir != -1)
	{
		calcNextGridDir(offset, cx, cy, nx, ny, gridDir);
		return;
	}
	
	int cur = 0;
	switch (dir & ~CannyThinFlag)
	{
		case Theta0:
			cur = 2;
			break;
		case Theta45:
			cur = 3;
			break;
		case Theta90:
			cur = 0;
			break;
		case Theta135:
			cur = 1;
			break;
	}
	
	cur += offset;
	if (cur < 0)  cur += 8;
	
	int ox = 0;
	int oy = 0;
	switch (cur)
	{
		case 0:
			ox = +1;  oy = 0;
			break;
		case 1:
			ox = +1;  oy = +1;
			break;
		case 2:
			ox = 0;  oy = +1;
			break;
		case 3:
			ox = -1;  oy = +1;
			break;
		case 4:
			ox = -1;  oy = 0;
			break;
		case 5:
			ox = -1;  oy = -1;
			break;
		case 6:
			ox = 0;  oy = -1;
			break;
		case 7:
			ox = +1;  oy = -1;
			break;
	}
	
	gridDir = cur;
	nx = cx + ox;
	ny = cy + oy;
}

// Check if the candidate pixel is acceptable to move to
// If thinOrNot is on, we'll just take thin edges
static bool checkPixel(int nx, int ny,RawImageGray32 *gradImg,RawImageGray32 *featImg,int featId,RawImageGray8 *thetaImg,int minThresh,int thinOrNot)
{
	// If the pixel's already been hit by us, it's fine.
	// If it's been hit, but not by this feature, can't go there
	int pixFeat = featImg->getPixel(nx, ny);
	if (pixFeat && pixFeat != featId)
		return false;
	
	// If we're just looking at thin edges, take that into account
	unsigned char dir = thetaImg->getPixel(nx,ny);
	if (thinOrNot)
		if (!(dir & CannyThinFlag))
			return false;
	
	return (dir != ThetaEmpty && gradImg->getPixel(nx,ny) > minThresh);
}


// Look for the next pixel given this one and a direction
// Return true if we found one, false otherwise
static bool findNext(RawImageGray32 *gradImg,RawImageGray32 *featImg,int featId,RawImageGray8 *thetaImg,int minThresh,int &cx,int &cy,int &gridDir,int &strayCount)
{
	bool isThin = false;
	int dir = thetaImg->getPixel(cx, cy);
	if (dir & CannyThinFlag)
	{
		isThin = true;
		dir &= ~CannyThinFlag;
	}
	int testDir; //=gridDir;

	int nx,ny;

	// -- Look for a thin edge ahead of us
	// --
	testDir = gridDir;
	calcNextThetaDir((ThetaAngles)(dir&~CannyThinFlag), 0, cx, cy, nx, ny,testDir);
	if (checkPixel(nx,ny,gradImg,featImg,featId,thetaImg,minThresh,1))
	{
		cx = nx;  cy = ny;  gridDir = testDir;
		return true;
	}

	// -- Look for an edge to the NE and NW (relative
	// --
	testDir = gridDir;
	calcNextThetaDir((ThetaAngles)(dir&~CannyThinFlag),1,cx,cy,nx,ny,testDir);
	if (checkPixel(nx,ny,gradImg,featImg,featId,thetaImg,minThresh,1))
	{
		cx = nx;  cy = ny;  gridDir = testDir;
		return true;
	}
	testDir = gridDir;
	calcNextThetaDir((ThetaAngles)(dir&~CannyThinFlag),-1,cx,cy,nx,ny,testDir);
	if (checkPixel(nx,ny,gradImg,featImg,featId,thetaImg,minThresh,1))
	{
		cx = nx; cy = ny;  gridDir = testDir;
		return true;
	}
		
	// -- Look for a thin edge on either side of the direction we're headed
	// -- (E,W) relative
	testDir = gridDir;
	calcNextThetaDir((ThetaAngles)(dir&~CannyThinFlag),2,cx,cy,nx,ny,testDir);
	if (checkPixel(nx,ny,gradImg,featImg,featId,thetaImg,minThresh,1))
	{
		cx = nx;  cy = ny;  gridDir = testDir;
		return true;
	}
	testDir = gridDir;
	calcNextThetaDir((ThetaAngles)(dir&~CannyThinFlag),-2,cx,cy,nx,ny,testDir);
	if (checkPixel(nx,ny,gradImg,featImg,featId,thetaImg,minThresh,1))
	{
		cx = nx; cy = ny;  gridDir = testDir;
		return true;
	}
	
	// -- Look for a thick edge ahead of us
	// --
	// N (relative)
	if (isThin||strayCount)
	{
		// We're only willing to stray a certain distance from a thin edge
		if (isThin)
			strayCount = 1;
		else
			strayCount--;
		testDir = gridDir;
		calcNextGridDir(0, cx, cy, nx, ny, testDir);
		if (checkPixel(nx,ny,gradImg,featImg,featId,thetaImg,minThresh,0))
		{
			cx = nx;  cy = ny;  gridDir = testDir;
			return true;
		}
		// Look NE and NW (relative)
		testDir = gridDir;
		calcNextGridDir(1,cx,cy,nx,ny,testDir);
		if (checkPixel(nx,ny,gradImg,featImg,featId,thetaImg,minThresh,0))
		{
			cx = nx;  cy = ny;  gridDir = testDir;
			return true;
		}
		testDir = gridDir;
		calcNextGridDir(-1,cx,cy,nx,ny,testDir);
		if (checkPixel(nx,ny,gradImg,featImg,featId,thetaImg,minThresh,0))
		{
			cx = nx;  cy = ny;  gridDir = testDir;
			return true;
		}
	}

	// -- Look for a thin edge behind us to the SE and SW
	// --
	testDir = gridDir;
	calcNextThetaDir((ThetaAngles)(dir&~CannyThinFlag),3,cx,cy,nx,ny,testDir);
	if (checkPixel(nx,ny,gradImg,featImg,featId,thetaImg,minThresh,1))
	{
		cx = nx;  cy = ny;  gridDir = testDir;
		return true;
	}
	testDir = gridDir;
	calcNextThetaDir((ThetaAngles)(dir&~CannyThinFlag),-3,cx,cy,nx,ny,testDir);
	if (checkPixel(nx,ny,gradImg,featImg,featId,thetaImg,minThresh,1))
	{
		cx = nx; cy = ny;  gridDir = testDir;
		return true;
	}
	
	// -- Only look S if we're just starting
	// --
	if (gridDir == -1)
	{
		// Look S
		testDir = gridDir;
		calcNextThetaDir((ThetaAngles)(dir&~CannyThinFlag), 4, cx, cy, nx, ny,testDir);
		if (checkPixel(nx,ny,gradImg,featImg,featId,thetaImg,minThresh,1))
		{
			cx = nx;  cy = ny;  gridDir = testDir;
			return true;
		}
	}
		
	// Didn't find anything around us
	return false;
}

// Decide if a pixel is too crowded to start
// It's too crowded if there's already two features nearby
bool pixelCrowded(RawImageGray32 *featImg,int cx,int cy)
{
	int nearCount = 0;
	
	for (unsigned int iy=cy+1;iy>=cy-1;iy--)
		for (unsigned int ix=cx-1;ix<=cx+1;ix++)
			if (featImg->getPixel(ix, iy))
				nearCount++;

	return nearCount >= 2;
}

// Erase the given feature from our image
// Note: Keep an MBR to make this faster
void ScrubFeature(RawImageGray8 *featImg,int featId)
{
	for (unsigned int iy=0;iy<featImg->getSizeY();iy++)
		for (unsigned int ix=0;ix<featImg->getSizeX();ix++)
		{
			unsigned char &val = featImg->getPixel(ix, iy);
			if (val == featId)
				val = 0;
		}
}

// Detected when we've closed a feature
bool closedFeature(RawImageGray32 *featImg,int featId,int cx,int cy,int gridDir)
{
	// Can't close a feature if we've just started
	if (gridDir == -1)
		return false;
	
	// Look for a feature pixel in the direction we're going
	int tmpGridDir=gridDir,nx,ny;
	calcNextGridDir(0,cx,cy,nx,ny,tmpGridDir);
	tmpGridDir=gridDir;
	if (featImg->getPixel(nx, ny) == featId)
		return true;
	//tmpGridDir=gridDir;
	calcNextGridDir(1,cx,cy,nx,ny,tmpGridDir);
	if (featImg->getPixel(nx, ny) == featId)
		return true;
	tmpGridDir=gridDir;
	calcNextGridDir(-1,cx,cy,nx,ny,tmpGridDir);
	if (featImg->getPixel(nx, ny) == featId)
		return true;
	
	return false;
}

// Don't bother with the pixels around the edge
const int FeatureOffset = 5,InnerOffset=4;

// Feature needs to have more than this number of pixels to matter
const int FeatureThreshhold = 10;

// Look for features using a min and max threshold.
// This function identifies features in an image by following gradients and edges.
void CannyFindFeatures(RawImageGray32 *gradImg, RawImageGray8 *thetaImg, int minThresh, int maxThresh, std::vector<Feature> &feats, RawImageGray32 *featImg)
{
    int featId = 1; // The ID of the feature being processed

    // Loop through the image pixels to search for features
    for (unsigned int iy = FeatureOffset; iy < gradImg->getSizeY() - FeatureOffset; iy++)
    {
        for (unsigned int ix = FeatureOffset; ix < gradImg->getSizeX() - FeatureOffset; ix++)
        {
            // Check if the pixel qualifies as part of a new feature:
            // - The gradient value is higher than the max threshold
            // - The pixel hasn't been visited
            // - There aren't too many neighboring features
            if ((thetaImg->getPixel(ix, iy) & CannyThinFlag) && !featImg->getPixel(ix, iy) && gradImg->getPixel(ix, iy) > maxThresh && !pixelCrowded(featImg, ix, iy))
            {
                int featCount = 0; // Count of pixels in this feature
                feats.resize(feats.size() + 1); // Add a new feature to the vector
                Feature &feat = feats[feats.size() - 1]; // Get reference to the new feature
                int cx = ix, cy = iy; // Starting point of the feature
                feat.addPointEnd(cx, cy); // Add the starting point to the feature

                // Debugging output
                logVerbose("Starting new feature at (" + std::to_string(ix) + ", " + std::to_string(iy) + "), feature ID: " + std::to_string(featId));

                // Follow the feature in one direction (forward)
                int gridDir = -1, startDir = -1;
                int startCx = cx, startCy = cy; // Store starting coordinates
                int strayCount = 1; // Allowable number of steps outside the edge

                // Follow the feature until it's no longer valid
                while (!featImg->getPixel(cx, cy) && gradImg->getPixel(cx, cy) > minThresh &&
                       cx > InnerOffset && cy > InnerOffset && cx < gradImg->getSizeX() - InnerOffset && cy < gradImg->getSizeY() - InnerOffset &&
                       !closedFeature(featImg, featId, cx, cy, gridDir))
                {
                    // Mark the pixel as part of this feature
                    featImg->getPixel(cx, cy) = featId;

                    // Try to find the next pixel in the feature
                    if (findNext(gradImg, featImg, featId, thetaImg, minThresh, cx, cy, gridDir, strayCount))
                    {
                        feat.addPointEnd(cx, cy); // Add the new point to the feature
                        featCount++;
                    }
                    else
                    {
                        break;
                    }

                    // Set startDir the first time we find the gridDir
                    if (startDir == -1)
                    {
                        startDir = gridDir;
                    }
                }

                // Debugging output
                logVerbose("First pass completed for feature ID: " + std::to_string(featId) + " with points: " + std::to_string(featCount));

                // Now follow the feature in the other direction (backward)
                cx = startCx;
                cy = startCy;
                gridDir = (startDir + 4) % 8; // Reverse direction
                strayCount = 1; // Reset stray count

                if (findNext(gradImg, featImg, featId, thetaImg, minThresh, cx, cy, gridDir, strayCount))
                {
                    feat.addPointBegin(cx, cy); // Add the starting point in reverse direction

                    while (!featImg->getPixel(cx, cy) && gradImg->getPixel(cx, cy) > minThresh &&
                           cx > InnerOffset && cy > InnerOffset && cx < gradImg->getSizeX() - InnerOffset && cy < gradImg->getSizeY() - InnerOffset &&
                           !closedFeature(featImg, featId, cx, cy, gridDir))
                    {
                        featImg->getPixel(cx, cy) = featId; // Mark the pixel as part of the feature

                        // Find the next pixel in reverse direction
                        if (findNext(gradImg, featImg, featId, thetaImg, minThresh, cx, cy, gridDir, strayCount))
                        {
                            feat.addPointBegin(cx, cy); // Add the point to the start of the feature
                            featCount++;
                        }
                        else
                        {
                            break;
                        }
                    }
                }

                // Debugging output
                logVerbose("Second pass completed for feature ID: " + std::to_string(featId) + " with total points: " + std::to_string(featCount));

                // Increment feature ID for the next feature
                featId++;
            }
        }
    }

    // Debugging output for the number of features detected
    logVerbose("Total features detected: " + std::to_string(featId - 1));
}

