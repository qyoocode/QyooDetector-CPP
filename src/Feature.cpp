/*
 *  Feature.mm
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/20/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#import <algorithm>

#import "Feature.h"
#import "CannyDetector.h"

// Tack a point on the end
void Feature::addPointEnd(int cx,int cy)
{
	points.push_back(Point(cx,cy));
}

// Insert at the beginning
void Feature::addPointBegin(int cx,int cy)
{
	points.push_front(Point(cx,cy));
}

// Decide if the shape is closed
// We'll take a "close enough" approch to this one
void Feature::calcClosed(int dist2)
{
	Point &p0 = points.front(),&p1 = points.back();
	
	int dx = p0.x - p1.x;
	int dy = p0.y - p1.y;
	
	closed =  (dx*dx + dy*dy) <= dist2;
}

// #define DECIMATEDEBUG 1

// Decimate points not "needed" to represent the overall shape
// Note: This has problems depending on where the start and end are
void Feature::decimate(float tol2)
{
#ifdef DECIMATEDEBUG
	printf("-----\n");
	for (std::list<Point>::iterator pt = points.begin();pt!=points.end();++pt)
		printf("Point = (%d,%d)\n",pt->x,pt->y);
	printf("-----\n");
#endif

	std::list<Point> newPoints = points;

	// Keep going while we're pulling points out
	// Note: Not sure why we have to do this multiple times.  Take a look.
	bool decimate = false;
	do
	{
		decimate = false;
		bool done = false;
		std::list<Point>::iterator pt0 = newPoints.begin();
		std::vector<Point> checkPts;  // The points we have to check against at each iteration
		while (newPoints.size() > 3 && !done)
		{
			
			// We need at least two more points
			// Pretend this is circular
			std::list<Point>::iterator cpt = pt0; cpt++;
			if (cpt == newPoints.end())  cpt = newPoints.begin();
			std::list<Point>::iterator pt2 = cpt; pt2++;
			checkPts.push_back(*cpt);
			if (pt2 == newPoints.end())  pt2 = newPoints.begin();
			
#ifdef DECIMATEDEBUG		
			printf("size = %d\n",newPoints.size());
#endif
			
			// We're back at the beginning, stop after this
			if (cpt == newPoints.begin())
				done = true;
			
			// Now we need to check the proposed line against every point we've eliminated
			bool passed = true;
#ifdef DECIMATEDEBUG
			printf("pt0 = (%d,%d), [%d points], pt1 = (%d,%d)\n  ",pt0->x,pt0->y,checkPts.size(),pt2->x,pt2->y);
#endif
			for (unsigned int ii=0;ii<checkPts.size();ii++)
			{
				Point &ipt = checkPts[ii];
				
				SimpleLineSegment seg(SimplePoint2D(pt0->x,pt0->y),SimplePoint2D(pt2->x,pt2->y));
				float dist = seg.dist2ToPtAsLine(SimplePoint2D(ipt.x,ipt.y));
				
#ifdef DECIMATEDEBUG
				printf("%f ",dist);
#endif
				
				if (dist > tol2)
				{
					passed = false;
					break;
				}
			}
#ifdef DECIMATEDEBUG
			printf("\n");
#endif
			
			// If the new line is close enough, delete this cpt and move on
			if (passed)
			{
				newPoints.erase(cpt);
				decimate = true;
			} else {
				pt0++;
				checkPts.clear();	
			}
		}
	} while (decimate);
		
#ifdef DECIMATEDEBUG
	printf("[%d new points]\n",newPoints.size());
#endif
	origPoints = points;
	points = newPoints;
}

// Calculate the center of mass and then pick the farthest point from that
// We're just using center of mass.  That will tend to favor the curved part
//  and push the center away from the corner
void Feature::findCorner()
{
	float centerX=0.0,centerY=0.0;
	
	for (std::list<Point>::iterator pt = points.begin();pt != points.end(); ++pt)
	{
		centerX += pt->x;  centerY += pt->y;
	}
	centerX /= points.size();
	centerY /= points.size();
	
	// Now look for the furthest point
	float maxDist2 = -1.0;
	cornerValid = false;
	for (std::list<Point>::iterator pt = points.begin();pt != points.end(); ++pt)
	{
		float dx = centerX - pt->x, dy = centerY - pt->y;
		float dist = dx*dx + dy*dy;
		if (dist > maxDist2)
		{
			cornerValid = true;
			cornX = pt->x;  cornY = pt->y;
			maxDist2 = dist;
		}
	}
}

// We don't want things too long and skinny
const float MinAspectRatio = 0.5;
// Or too small
const float MinAreaFraction = 0.03,MaxAreaFraction = 1.0; 

// Calculate the overall size and check against the size of
// the image as well as the overall aspect ratio of the feature
void Feature::checkSizeAndPosition(int imgSizeX,int imgSizeY)
{
	// Calculate the MBR
	int minX = imgSizeX,minY = imgSizeY;
	int maxX = -1,maxY = -1;
	for (std::list<Point>::iterator pt = points.begin();pt != points.end(); ++pt)
	{
		if (pt->x < minX)  minX = pt->x;
		if (pt->y < minY)  minY = pt->y;
		if (pt->x > maxX)  maxX = pt->x;
		if (pt->y > maxY)  maxY = pt->y;
	}
	
	int sizeX = maxX - minX, sizeY = maxY - minY;
	
	// Only put up with an aspect ratio of 1:2 or so
	if ((sizeX == 0 || sizeY == 0) ||
		(sizeX > sizeY && (float)sizeX / (float)sizeY < MinAspectRatio) ||
		(sizeY > sizeX && (float)sizeY / (float)sizeX < MinAspectRatio))
	{
		valid = false;
		return;
	}
	
	// Now for an overall area check
	float totalArea = imgSizeX*imgSizeY;
	float featArea = sizeX*sizeY;
	float areaFrac = featArea / totalArea;
	if (areaFrac < MinAreaFraction || areaFrac > MaxAreaFraction)
		valid = false;
}

// Simple class for edges we're comparing below
class LongEdge
{
public:
	LongEdge() { }
	LongEdge(int isx,int isy,int iex,int iey) { sx = isx; sy = isy; ex = iex; ey = iey; }
	
	// Calculate (or used cached) length
	float len2() const
	{
		float dx = ex-sx, dy = ey-sy;
		return (dx*dx + dy*dy);
	}
	
	// Return in angle from 0->360
	// Note: Could be simpler
	float calcAngle() const
	{
		return properAtan(ex-sx, ey-sy);
	}

	int sx,sy,ex,ey;  // sx,sy is the point closest to the corner
};

// Comparison for long edges
static bool compFunction (const LongEdge &a,const LongEdge &b) 
{ 
	return a.len2() > b.len2();
}

// Look for the point farthest from the given line
bool Feature::findFarPoint(float p0x,float p0y,float p1x,float p1y,Point &far,float &farDist2)
{
	float maxDist2 = -1.0;
	
	// We just care about the points here
	for (std::list<Point>::iterator pt = origPoints.begin(); pt != origPoints.end(); ++pt)
	{
		SimpleLineSegment seg(SimplePoint2D(p0x,p0y),SimplePoint2D(p1x,p1y));
		float dist2 = seg.dist2ToPtAsLine(SimplePoint2D(pt->x,pt->y));
		if (dist2 > maxDist2)
		{
			maxDist2 = dist2;
			far.x = pt->x;
			far.y = pt->y;
		}
	}
	
	farDist2 = maxDist2;
	return maxDist2 > 0.0;
}

/* Pick up the longest two edges near the corner.
   Compare the angles and, if they're far enough apart,
    make them the new edges.
 */
void Feature::refineCornerAndFindAngles(int searchDist2)
{
	std::vector<LongEdge> edges;
	
	bool done = false;
	std::list<Point>::iterator p0 = points.begin();
	std::list<Point>::iterator p1 = p0;  p1++;
	while (!done)
	{
		// See which end is closer
		float dxA = cornX - p0->x, dyA = cornY - p0->y;
		float dxB = cornX - p1->x, dyB = cornY - p1->y;
		float distA = dxA*dxA + dyA*dyA, distB = dxB*dxB + dyB*dyB;
		if (distA < distB && distA < searchDist2)
		{
			LongEdge edge(p0->x,p0->y,p1->x,p1->y);
			edges.push_back(edge);
		} else
			if (distB < distA && distB < searchDist2)
			{
				LongEdge edge(p1->x,p1->y,p0->x,p0->y);
				edges.push_back(edge);
			}
		
		p0 = p1;  p1++;
		if (closed)
		{
			if (p1 == points.end())
				p1 = points.begin();
			done = (p0 == points.begin());
		}
		else
			done = (p1 == points.end());
	}
	
	// Start out invalid and see where we go
	edgesValid = false;
	if (edges.size() >= 2)
	{
		std::sort(edges.begin(),edges.end(),compFunction);
		LongEdge &edge0 = edges[0], &edge1 = edges[1];
		
		// Take a look at the angles and decide if they're too far apart
		ang0 = edge0.calcAngle();
		ang1 = edge1.calcAngle();
		float angDiff = ang1 - ang0;  angDiff = abs(angDiff);
		if (angDiff > 180.0)  angDiff -= 180.0;
		if (angDiff > 65 && angDiff < 125)
		{
			// Store the edges
			edgesValid = true;
			e0.x = edge0.ex;  e0.y = edge0.ey;
			e1.x = edge1.ex;  e1.y = edge1.ey;
			
			// And calculate a shiny new corner point
			// Note: Reassigning variable names because I'm lazy
			float x1 = edge0.sx, y1 = edge0.sy;
			float x2 = edge0.ex, y2 = edge0.ey;
			float x3 = edge1.sx, y3 = edge1.sy;
			float x4 = edge1.ex, y4 = edge1.ey;
			float denom = ((x1-x2)*(y3-y4) - (y1-y2)*(x3-x4));
			cornX = ((x1*y2 - y1*x2)*(x3-x4) - (x1-x2)*(x3*y4 - y3*x4))/denom;
			cornY = ((x1*y2 - y1*x2)*(y3-y4) - (y1-y2)*(x3*y4 - y3*x4))/denom;
			
			// Calculate the Z portion of a cross product and switch the edges
			//  if it's not pointing up
			if ((e0.x-cornX) * (e1.y-cornY) - (e0.y-cornY) * (e1.x-cornX) < 0)
			{
				Point tmpPt = e1;
				e1 = e0;  e0 = tmpPt;
				float tmpAng = ang1;
				ang1 = ang0;  ang0 = tmpAng;
			}
			
			// Calculate a sheer value to account for the skew.  Sort of.
			// We want a unit vector 90 degrees ahead of e0
			float modelAng = ang0+90.0;
			if (modelAng > 360)  modelAng -= 360;
			float mdir_x = cosf(modelAng*M_PI/180.0), mdir_y = sinf(modelAng*M_PI/180.0);
			float e1len = sqrtf((e1.x-cornX)*(e1.x-cornX)+(e1.y-cornY)*(e1.y-cornY));
			float udir_x = (e1.x-cornX) / e1len, udir_y = (e1.y-cornY) / e1len;
			// Distance between the points at the end of the unit vectors is our sheer magnitude
			sheer = sqrtf((mdir_x-udir_x)*(mdir_x-udir_x) + (mdir_y-udir_y)*(mdir_y-udir_y));
			// Depending on the angle difference it'll be positive or negative
			float angComp = ang1-ang0;  if (angComp < 0)  angComp += 360;
			if (angComp > 90.0)
				sheer *= -1;
			
			// Now look for the outer extents of shape
			if (findFarPoint(cornX,cornY,e0.x,e0.y,far0,dist0) &&
				findFarPoint(cornX,cornY,e1.x,e1.y,far1,dist1))
				farEdgesValid = true;		
			
			// Build up a matrix if we found everything we want
			if (farEdgesValid)
			{
				float scaleX = sqrtf(dist1), scaleY = sqrtf(dist0);

				QyooMatrix sheerMat(1.0, sheer, 0.0,
						    0.0, 1.0, 0.0,
						    0.0, 0.0, 1.0);
				QyooMatrix transMat(1.0, 0.0, cornX,
						    0.0, 1.0, cornY,
						    0.0, 0.0, 1.0);
				double angle = ang0 * M_PI / 180.0;
				QyooMatrix rotMat(cos(angle), -sin(angle), 0.0,
						  sin(angle), cos(angle), 0.0,
						  0.0, 0.0, 1.0);
				QyooMatrix scaleMat(scaleX, 0.0, 0.0,
						    0.0, scaleY, 0.0,
						    0.0, 0.0, 1.0);

				mat = transMat;
				mat = mat * rotMat;
				mat = mat * scaleMat;
                mat = mat * sheerMat;
				
			}
		}
	}

	valid = edgesValid && farEdgesValid;
}

// Check a single point against the ideal model
bool Feature::pointModelCheck(QyooMatrix *invTrans,float imgX,float imgY,float nearDist2)
{
        cml::vector3d pt = (*invTrans) * cml::vector3d(imgX,imgY,1.0);
  	SimplePoint2D pt2d(pt[0],pt[1]);
	
	// Is it close to the bottom line
	SimpleLineSegment bline(SimplePoint2D(0.0,0.0),SimplePoint2D(0.5,0.0));
	if (bline.dist2ToPt(pt2d) < nearDist2)
		return true;
	
	// How about the left line
	SimpleLineSegment lline(SimplePoint2D(0.0,0.0),SimplePoint2D(0.0,0.5));
	if (lline.dist2ToPt(pt2d) < nearDist2)
		return true;
	
	// Now check a circle centered at (0.5,0.5)
	pt2d.x -= 0.5;  pt2d.y -= 0.5;
	
	// Not if it's in sector III
	if (pt2d.x > 0 || pt2d.y > 0)
	{
		// Compare the "radius" of our point to the circle with a radius of 0.5
		float ptRad = sqrtf(pt2d.x*pt2d.x+pt2d.y*pt2d.y);
		float dist = fabs(ptRad-0.5);
		return (dist*dist < nearDist2);
	}
			
	return false;
}

/* Check against the model
	We'll run all the original points through the transform and make sure
     they're close to where we think they ought to be.
    There's a distance each point should be within and a fraction we expect
     to be within that distance.
 */
bool Feature::modelCheck(float nearDist2,float nearFrac)
{
	if (!valid)
		return false;

	QyooMatrix invMat = mat;
	invMat.inverse();

	// Work through the original points
	int numClose = 0;
	int total = 0;
	for (std::list<Point>::iterator pt = origPoints.begin();pt != origPoints.end();++pt)
	{
		if (pointModelCheck(&invMat,pt->x,pt->y,nearDist2))
			numClose++;
		total++;
	}
	
	modelChecked = ((float)numClose/ (float) total > nearFrac);
	
	if (!modelChecked)
		valid = false;
	return modelChecked;
}
