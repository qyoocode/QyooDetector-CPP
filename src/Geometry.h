/*
 *  Geometry.h
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/28/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#include <math.h>

// Just a 3D point class
class SimplePoint3D
{
public:
	SimplePoint3D() { };
	SimplePoint3D(float inX,float inY,float inZ) { this->x = inX;  this->y = inY;  this->z = inZ; }
	SimplePoint3D(const SimplePoint3D &that) { this->x = that.x;  this->y = that.y;  this->z = that.z; }
		
	float x,y,z;
};

// The math's easier to follow if we do this
class SimplePoint2D
{
public:
	SimplePoint2D() { };
	SimplePoint2D(float inX,float inY) { this->x = inX; this->y = inY; }
	SimplePoint2D(const SimplePoint2D &that) { this->x = that.x; this->y = that.y;}
		
	// Dot product
	inline float operator * (const SimplePoint2D &that) { return this->x * that.x + this->y * that.y; }
		
	// Cross product
	inline SimplePoint3D operator ^ (const SimplePoint2D &that) const
	{
		return SimplePoint3D(0,0,this->x*that.y - this->y*that.x);
	}
	
	// Scale
	inline SimplePoint2D & operator *= (float scale)
	{
		this->x *= scale;  this->y *= scale;
		return *this;
	}
	
	// Scale
	inline SimplePoint2D operator * (float scale) const
	{
		return SimplePoint2D(this->x * scale, this->y * scale);
	}
		
	// Distance^2 from this point to another
	inline float dist2(const SimplePoint2D &that) const
	{
		SimplePoint2D dir(that.x-x,that.y-y);
		return dir.x * dir.x + dir.y * dir.y;
	}
	
	// Addition
	inline SimplePoint2D operator + (const SimplePoint2D &that) const
	{
		return SimplePoint2D(x+that.x,y+that.y);
	}
	
	// Normalize
	inline void norm()
	{
		float len = sqrtf(x*x + y*y);
		if (len != 0.0)
		{
			x /= len;  y /= len;
		}
	}
		
	float x,y;
};

// Again, makes the math easier to read
class SimpleLineSegment
{
public:
	SimpleLineSegment() { };
	SimpleLineSegment(const SimpleLineSegment &that) { this->p0 = that.p0; this->p1 = that.p1; }
	SimpleLineSegment(const SimplePoint2D &inP0,const SimplePoint2D &inP1) { p0 = inP0;  p1 = inP1; }
	
	// Distance^2 from this line segment to a point
	float dist2ToPt(const SimplePoint2D &pt) const
	{
		// Well, we need the parametric value first
		SimplePoint2D dir(p1.x-p0.x,p1.y-p0.y);
		float t = ((pt.x - p0.x) * dir.x + (pt.y - p0.y) * dir.y) / (dir.x * dir.x + dir.y * dir.y);
		if (t < 0)
		{
			return p0.dist2(pt);
		} else
			if (t >= 1.0)
			{
				return p1.dist2(pt);
			} else {
				// Distance to intersection point
				SimplePoint2D ipt(p0.x + t*dir.x,p0.y + t*dir.y);
				return ipt.dist2(pt);
			}
	}
	
	// Distance^2 from line to point.  Don't take ends into account
	float dist2ToPtAsLine(const SimplePoint2D &ipt) const
	{
		SimplePoint2D dir0(p1.x-p0.x,p1.y-p0.y);
		
		// pt1 distance from pt0->pt2
		float dist0 = dir0.x*dir0.x + dir0.y*dir0.y;
		
		SimplePoint2D dir1(p0.x-ipt.x,p0.y-ipt.y);
		float dist1 = dir1.x*dir1.x + dir1.y*dir1.y;
		
		float dot =  (dir1.x*dir0.x + dir1.y*dir0.y);
		return (dist1 * dist0 - dot * dot)/dist0;
	}
	
	SimplePoint2D p0,p1;
};

// Atan that takes quadrant into account correctly
extern float properAtan(int gx,int gy);

