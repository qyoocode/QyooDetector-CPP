/*
 *  Geometry.mm
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/28/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#include "Geometry.h"

// Break it down by quadrant.  Returns degrees for now.
float properAtan(int gx,int gy)
{
	float fgx = fabs(gx);
	float fgy = fabs(gy);
	
	if (gx > 0)
	{
		if (gy > 0)
		{
			// Quadrant I
			return atan(fgy/fgx)*180.0/M_PI;
		} else {
			// Quadrant IV
			return atan(fgx/fgy)*180.0/M_PI+270.0;
		}
	} else {
		if (gx == 0)
		{
			// Special case along the Y axis
			if (gy < 0)
				return 270.0;
			else
				return 90.0;
		} else
		{
			if (gy > 0)
			{
				// Quadrant II
				return atan(fgx/fgy)*180.0/M_PI+90.0;
			} else {
				// Quadrant III
				return atan(fgy/fgx)*180.0/M_PI+180.0;
			}
		}
	}
}
