/*
 *  Geometry.h
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/28/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 *  This file provides basic geometric utilities, including 2D and 3D point classes,
 *  line segment class for geometric calculations, and a utility to compute the
 *  arctangent while considering quadrant information.
 */

#include <math.h>

// 3D Point Class
// Simple representation of a 3D point with x, y, and z coordinates.
class SimplePoint3D
{
public:
    // Default constructor
    SimplePoint3D() { };

    // Constructor with coordinates
    SimplePoint3D(float inX, float inY, float inZ)
    {
        this->x = inX;
        this->y = inY;
        this->z = inZ;
    }

    // Copy constructor
    SimplePoint3D(const SimplePoint3D &that)
    {
        this->x = that.x;
        this->y = that.y;
        this->z = that.z;
    }

    // Coordinates
    float x, y, z;
};

// 2D Point Class
// Simple 2D point representation with basic geometric operations such as dot product,
// cross product, scaling, distance, and normalization.
class SimplePoint2D
{
public:
    // Default constructor
    SimplePoint2D() { }

    // Constructor with coordinates
    SimplePoint2D(float inX, float inY)
    {
        this->x = inX;
        this->y = inY;
    }

    // Copy constructor
    SimplePoint2D(const SimplePoint2D &that)
    {
        this->x = that.x;
        this->y = that.y;
    }

    // Dot product
    inline float operator * (const SimplePoint2D &that)
    {
        return this->x * that.x + this->y * that.y;
    }

    // Cross product (returns a SimplePoint3D result)
    inline SimplePoint3D operator ^ (const SimplePoint2D &that) const
    {
        return SimplePoint3D(0, 0, this->x * that.y - this->y * that.x);
    }

    // Scale by a scalar (modifies the current object)
    inline SimplePoint2D &operator *= (float scale)
    {
        this->x *= scale;
        this->y *= scale;
        return *this;
    }

    // Scale by a scalar (returns a new object)
    inline SimplePoint2D operator * (float scale) const
    {
        return SimplePoint2D(this->x * scale, this->y * scale);
    }

    // Compute squared distance between this point and another point
    inline float dist2(const SimplePoint2D &that) const
    {
        SimplePoint2D dir(that.x - x, that.y - y);
        return dir.x * dir.x + dir.y * dir.y;
    }

    // Addition of two points
    inline SimplePoint2D operator + (const SimplePoint2D &that) const
    {
        return SimplePoint2D(x + that.x, y + that.y);
    }

    // Normalize the point (make it a unit vector)
    inline void norm()
    {
        float len = sqrtf(x * x + y * y);
        if (len != 0.0)
        {
            x /= len;
            y /= len;
        }
    }

    // Coordinates
    float x, y;
};

// Line Segment Class
// Represents a line segment between two points and provides methods for distance
// calculations and comparisons.
class SimpleLineSegment
{
public:
    // Default constructor
    SimpleLineSegment() { }

    // Copy constructor
    SimpleLineSegment(const SimpleLineSegment &that)
    {
        this->p0 = that.p0;
        this->p1 = that.p1;
    }

    // Constructor with two endpoints
    SimpleLineSegment(const SimplePoint2D &inP0, const SimplePoint2D &inP1)
    {
        p0 = inP0;
        p1 = inP1;
    }

    // Compute squared distance from this line segment to a point
    float dist2ToPt(const SimplePoint2D &pt) const
    {
        SimplePoint2D dir(p1.x - p0.x, p1.y - p0.y);
        float t = ((pt.x - p0.x) * dir.x + (pt.y - p0.y) * dir.y) / (dir.x * dir.x + dir.y * dir.y);

        if (t < 0)  // Closest to p0
        {
            return p0.dist2(pt);
        }
        else if (t >= 1.0)  // Closest to p1
        {
            return p1.dist2(pt);
        }
        else  // Closest to the line segment between p0 and p1
        {
            SimplePoint2D ipt(p0.x + t * dir.x, p0.y + t * dir.y);
            return ipt.dist2(pt);
        }
    }

    // Compute squared distance from an infinite line to a point (ignores segment endpoints)
    float dist2ToPtAsLine(const SimplePoint2D &ipt) const
    {
        SimplePoint2D dir0(p1.x - p0.x, p1.y - p0.y);
        float dist0 = dir0.x * dir0.x + dir0.y * dir0.y;

        SimplePoint2D dir1(p0.x - ipt.x, p0.y - ipt.y);
        float dist1 = dir1.x * dir1.x + dir1.y * dir1.y;

        float dot = (dir1.x * dir0.x + dir1.y * dir0.y);
        return (dist1 * dist0 - dot * dot) / dist0;
    }

    // Endpoints of the line segment
    SimplePoint2D p0, p1;
};

// Utility function: Compute arctangent considering the correct quadrant
// Computes the angle in degrees relative to the positive x-axis, considering
// which quadrant the point (gx, gy) falls into.
extern float properAtan(int gx, int gy);
