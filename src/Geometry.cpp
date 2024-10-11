/*
 *  Geometry.mm
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/28/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#include "Geometry.h"
#include <cmath>

/**
 * Compute the proper arctangent for a given point in Cartesian coordinates.
 *
 * This function determines the angle (in degrees) of the vector (gx, gy) relative to the positive x-axis.
 * It handles the different quadrants (I-IV) of the Cartesian plane to return a correct result.
 * The returned angle is always between 0 and 360 degrees.
 *
 * @param gx The x-component of the vector (can be negative or zero).
 * @param gy The y-component of the vector (can be negative or zero).
 * @return The angle in degrees relative to the positive x-axis, in the range [0, 360).
 */
float properAtan(int gx, int gy)
{
    // Convert gx and gy to absolute values for calculation
    float fgx = fabs(gx);
    float fgy = fabs(gy);

    // Determine the angle based on the quadrant
    if (gx > 0)
    {
        if (gy > 0)
        {
            // Quadrant I: both gx and gy are positive
            return atan(fgy / fgx) * 180.0 / M_PI;
        }
        else
        {
            // Quadrant IV: gx > 0, gy < 0
            return atan(fgx / fgy) * 180.0 / M_PI + 270.0;
        }
    }
    else if (gx == 0)
    {
        // Special case: the point lies on the y-axis
        if (gy < 0)
        {
            // Negative y-axis direction
            return 270.0;
        }
        else
        {
            // Positive y-axis direction
            return 90.0;
        }
    }
    else // gx < 0
    {
        if (gy > 0)
        {
            // Quadrant II: gx < 0, gy > 0
            return atan(fgx / fgy) * 180.0 / M_PI + 90.0;
        }
        else
        {
            // Quadrant III: both gx and gy are negative
            return atan(fgy / fgx) * 180.0 / M_PI + 180.0;
        }
    }
}
