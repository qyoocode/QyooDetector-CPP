/*
 *  Convolution.cpp
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/15/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#include "Convolution.h"

// Construct a convolution filter (int version)
ConvolutionFilterInt::ConvolutionFilterInt(int size)
{
	this->size = size;
	filter = new int[size*size];
}

ConvolutionFilterInt::~ConvolutionFilterInt()
{
	delete [] filter;
}

// Process one image into another
// These should be the same size
void ConvolutionFilterInt::processImage(RawImageGray8 *inImg,RawImageGray8 *outImg)
{
	int halfSize = size/2;
	int startX = halfSize,startY = halfSize;
	int endX = inImg->getSizeX()-halfSize, endY = inImg->getSizeY()-halfSize;
	
	for (unsigned int ix=startX;ix<endX;ix++)
		for (unsigned int iy=startY;iy<endY;iy++)
		{
			// ix,iy is the destination pixel
			int sum = 0;
			for (unsigned int f_iy=0;f_iy<size;f_iy++)
				for (unsigned int f_ix=0;f_ix<size;f_ix++)
					sum += inImg->getPixel(f_ix+ix-halfSize, f_iy+iy-halfSize) * getEl(f_ix,f_iy);
			if (factor != 1)
				sum /= factor;
			if (sum > 255) 
				sum = 255;
			outImg->getPixel(ix, iy) = sum;
		}
}

// Process one image into another
// These should be the same size
void ConvolutionFilterInt::processImage(RawImageGray8 *inImg,RawImageGray32 *outImg)
{
	int halfSize = size/2;
	int startX = halfSize,startY = halfSize;
	int endX = inImg->getSizeX()-halfSize, endY = inImg->getSizeY()-halfSize;
	
	for (unsigned int ix=startX;ix<endX;ix++)
		for (unsigned int iy=startY;iy<endY;iy++)
		{
			// ix,iy is the destination pixel
			int sum = 0;
			for (unsigned int f_iy=0;f_iy<size;f_iy++)
				for (unsigned int f_ix=0;f_ix<size;f_ix++)
					sum += inImg->getPixel(f_ix+ix-halfSize, f_iy+iy-halfSize) * getEl(f_ix,f_iy);
			if (factor != 1)
				sum /= factor;
			outImg->getPixel(ix, iy) = sum;
		}
}

// Process a single pixel and return the results in an array
void ConvolutionFilterInt::processPixel(RawImageGray8 *inImg,int px,int py,int *results)
{
	int ir=0;
	int halfSize = size/2;
	for (unsigned int iy=0;iy<size;iy++)
		for (unsigned int ix=0;ix<size;ix++)
		{
			unsigned char pix = inImg->getPixel(px+ix-halfSize,py+iy-halfSize);
			unsigned char el = getEl(ix,iy);
			if (el)
				results[ir++] =  pix * el;
			else
				results[ir++] = -1;
		}
}

// Print out the filter
void ConvolutionFilterInt::print(FILE *fp)
{
	fprintf(fp,"Filter Size (%d,%d)\n",size,size);
	for (unsigned int iy=0;iy<size;iy++)
	{
		fprintf(fp,"  ");
		for (unsigned int ix=0;ix<size;ix++)
			fprintf(fp,"%d ",getEl(ix,iy));
		fprintf(fp,"\n");
	}
}

// Build a general purpose gaussian filter
ConvolutionFilterInt *MakeGaussianFilter(int size,float sigma)
{
	ConvolutionFilterInt *filter = new ConvolutionFilterInt(size);
	int halfSize = size/2;
	float *vals = new float[size*size];
	
	float sum = 0.0;
	float maxVal=-10e6,minVal=10e6;
	for (unsigned int iy=0;iy<size;iy++)
		for (unsigned int ix=0;ix<size;ix++)
		{
			int px = ix-halfSize, py = iy-halfSize;
			float g = 1.0/(2*M_PI*sigma*sigma)*expf(-1.0*(px*px+py*py)/(2*sigma*sigma));
			vals[iy*size+ix] = g;
			sum += g;
			if (g < minVal)  minVal = g;
			if (g > maxVal)  maxVal = g;
		}
	
	// Now run through and scale everything up
	int iSum = 0;
	for (unsigned int iy=0;iy<size;iy++)
		for (unsigned int ix=0;ix<size;ix++)
		{
			float g = vals[iy*size+ix];
			int ig = 10.0*g/minVal;
			iSum += ig;
			filter->getEl(ix, iy) = ig;
		}
	filter->getFact() = iSum;
			
	
	return filter;
}

// Make a hardwired gaussian filter
int preGausFilter[] = {2,4,5,4,2, 4,9,12,9,4, 5,12,15,12,5, 4,9,12,9,4, 2,4,5,4,2};
ConvolutionFilterInt *MakeGaussianFilter_1_4()
{
	ConvolutionFilterInt *filter = new ConvolutionFilterInt(5);
	filter->getFact() = 115;
	for (unsigned int ii=0;ii<5*5;ii++)
		filter->getFilter()[ii] = preGausFilter[ii];
	
	return filter;
}

// Sobel filter in X
int preSobelX[] = {-1,0,1, -2,0,2, -1,0,1};
ConvolutionFilterInt *MakeSobelFilterX()
{
	ConvolutionFilterInt *filter = new ConvolutionFilterInt(3);
	filter->getFact() = 1;
	for (unsigned int ii=0;ii<3*3;ii++)
		filter->getFilter()[ii] = preSobelX[ii];
		
	return filter;
}

// Sobel filter in Y
int preSobelY[] = {-1,-2,-1, 0,0,0, 1,2,1};
ConvolutionFilterInt *MakeSobelFilterY()
{
	ConvolutionFilterInt *filter = new ConvolutionFilterInt(3);
	filter->getFact() = 1;
	for (unsigned int ii=0;ii<3*3;ii++)
		filter->getFilter()[ii] = preSobelY[ii];
	
	return filter;
}

// Make a sizeXsize filter that contains ones within radius
ConvolutionFilterInt *MakeRadiusFilter(int size, int radius)
{
	ConvolutionFilterInt *filter = new ConvolutionFilterInt(size);
	
	filter->getFact() = 0;
	int cx = size/2, cy = size/2;
	for (unsigned int ix=0;ix<size;ix++)
		for (unsigned int iy=0;iy<size;iy++)
		{
			int dx = ix-cx, dy = iy-cy;
			int elVal = (dx*dx + dy*dy < radius*radius);
			filter->getEl(ix,iy) = elVal;
			filter->getFact() += elVal;
		}
	
	return filter;
}

// Make a 3x3 filter that doesn't do anything
int preIdentFilter[] = {0, 0, 0, 0, 1, 0, 0, 0, 0};
ConvolutionFilterInt *MakeIdentityFilter()
{
	ConvolutionFilterInt *filter = new ConvolutionFilterInt(3);
	filter->getFact() = 1;
	for (unsigned int ii=0;ii<3*3;ii++)
		filter->getFilter()[ii] = preIdentFilter[ii];
	
	return filter;	
}
