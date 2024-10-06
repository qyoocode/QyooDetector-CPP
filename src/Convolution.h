/*
 *  Convolution.h
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/15/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#import "RawImage.h"
#import <math.h>

/* Convolution Filter
	Encapsulates your basic convolution filter.
	Integer version.
 */
class ConvolutionFilterInt
{
public:
	// Initialize a convolution filter of the given size
	// Size needs to be odd
	ConvolutionFilterInt(int size);
	~ConvolutionFilterInt();
	
	// Reference to an element of the filter
	inline int &getEl(int x,int y) { return filter[y*size + x]; }
	inline int *getFilter() { return filter; }
	inline int &getFact() { return factor; }
	inline int getSize() { return size; }
	
	// Run the convolution filter on the input image
	// Store the results in the output image
	// Note: Yeah, this could be more efficient
	void processImage(RawImageGray8 *inImg,RawImageGray8 *outImg);
	void processImage(RawImageGray8 *inImg,RawImageGray32 *outImg);
	
	// Calculate the results around the given pixel and return them
	// Results needs enough space to do that
	// Returns -1 in the results if the filter was zero at that point
	void processPixel(RawImageGray8 *inImg,int px,int py,int *results);
			
	// Print out the filter (for debugging)
	void print(FILE *fp);
protected:
	int size;
	int factor;  // Divide by this at the end
	int *filter;
};

// Make a hardwired Gaussian filter, sigma = 1.4
// This has been tuned to work well, even though it ups the data values a little
ConvolutionFilterInt *MakeGaussianFilter_1_4();

// Build up a gaussian convolution filter of the given size
// Size should be odd, sigma is the parameter
ConvolutionFilterInt *MakeGaussianFilter(int size,float sigma);

// Sobel operator in X
ConvolutionFilterInt *MakeSobelFilterX();

// Sobel operator in Y
ConvolutionFilterInt *MakeSobelFilterY();

// A filter that contains ones within the given radius
ConvolutionFilterInt *MakeRadiusFilter(int size,int radius);

// A filter that does nothing
ConvolutionFilterInt *MakeIdentityFilter();

