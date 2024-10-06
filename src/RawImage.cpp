/*
 *  RawImage.cpp
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/15/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#include "RawImage.h"

// Save out image for debugging
void gdSaveAsPng(gdImagePtr theImage,const char *fileName)
{
  FILE *fp = fopen(fileName,"wb");
  if (!fp)  return;

  gdImagePng(theImage,fp);
  fclose(fp);
}

// Flip and return the gd image
gdImagePtr gdFlipImage(gdImagePtr theImage)
{
  gdImagePtr outImg = gdImageCreate(gdImageSY(theImage),gdImageSX(theImage));
  for (unsigned int ic=0;ic<255;ic++)
    gdImageColorAllocate(outImg,ic,ic,ic);

  for (unsigned int ix=0;ix<gdImageSX(theImage);ix++)
    for (unsigned int iy=0;iy<gdImageSY(theImage);iy++)
      {
	int pixVal = gdImageRed(theImage,gdImageGetPixel(theImage,ix,iy));
	int red = gdImageRed(theImage,pixVal);
	int green = gdImageGreen(theImage,pixVal);
	int blue = gdImageBlue(theImage,pixVal);
	int outVal = (red+green+blue)/3;
	gdImageSetPixel(outImg,iy,ix,outVal);
      }

  return outImg;
}

// Construct with a new image
RawImageGray8::RawImageGray8(int sizeX,int sizeY)
{
	this->sizeX = sizeX;  this->sizeY = sizeY;
	isMine = true, useFree = false;
	img = new unsigned char [sizeX*sizeY];
	memset(img,0,totalSize());
}

// Destructor
RawImageGray8::~RawImageGray8()
{
	if (isMine)
	{
		if (useFree)
			free(img);
		else
			delete [] img;
	}
	img = NULL;
}

// Pull the data out of a GD Image
void RawImageGray8::copyFromGDImage(gdImagePtr inImage)
{
  gdImagePtr tmpImg = gdImageCreate(sizeX,sizeY);
  for (unsigned int ic=0;ic<255;ic++)
    gdImageColorAllocate(tmpImg,ic,ic,ic);
  gdImageCopyResampled(tmpImg,inImage,0,0,0,0,sizeX,sizeY,gdImageSX(inImage),gdImageSY(inImage));
  //  gdSaveAsPng(tmpImg,"gdInput.png");

  // Now pull out the data
  for (unsigned ix=0;ix<tmpImg->sx;ix++)
    for (unsigned int iy=0;iy<tmpImg->sy;iy++)
      {
	// We're assuming the input images are grayscale
	int pixVal = gdImageGetPixel(tmpImg,ix,iy);
	int red = gdImageRed(tmpImg,pixVal);
	int green = gdImageGreen(tmpImg,pixVal);
	int blue = gdImageGreen(tmpImg,pixVal);
	int gray = (red+green+blue)/3;
	getPixel(ix,iy) = gdImageRed(tmpImg,gray);
	if (pixVal != 0) 
	  {
	    int foo = 0;
	  }
      }

  gdImageDestroy(tmpImg);
}

// Pull the data out of a GDImage, but apply a transform
void RawImageGray8::copyFromGDImage(gdImagePtr inImage,QyooMatrix &mat)
{
  QyooMatrix invMat = mat;
  invMat.inverse();

  cml::vector3d pt0 = invMat * cml::vector3d(0,0,1.0);
  cml::vector3d pt1 = invMat * cml::vector3d(1,1,1.0);

  for (unsigned int ix=0;ix<sizeX;ix++)
    for (unsigned int iy=0;iy<sizeY;iy++)
      {
	float ixP = (float)ix/(float)sizeX;
	float iyP = (float)iy/(float)sizeY;
	// Project this pixel back to see where it falls
	cml::vector3d pt = invMat * cml::vector3d(ixP,iyP,1.0);
	int destX = pt[0]+0.5, destY = pt[1]+0.5;
	// Snap to the boundaries of the source
	if (destX < 0)  destX = 0;
	if (destX >= gdImageSX(inImage)) destX = gdImageSX(inImage)-1;
	if (destY < 0)  destY = 0;
	if (destY >= gdImageSY(inImage)) destY = gdImageSY(inImage)-1;

	// And assign the pixel
	// Note: This might look a little pixelated, but should work for out purposes
	//       Might also be off by half a pixel
	int pixVal = gdImageGetPixel(inImage,destX,destY);
	getPixel(ix,iy) = gdImageRed(inImage,pixVal);
      }
}

// Construct a GD Image out of the raw data
gdImagePtr RawImageGray8::makeGDImage()
{
  gdImagePtr newImage = gdImageCreate(sizeX,sizeY);
  for (unsigned int ic=0;ic<255;ic++)
    gdImageColorAllocate(newImage,ic,ic,ic);

  for (unsigned int ix=0;ix<sizeX;ix++)
    for (unsigned int iy=0;iy<sizeY;iy++)
      {
	gdImageSetPixel(newImage,ix,iy,getPixel(ix,iy));
      }

  return newImage;
}

// Run a simple contrast scaling operation
void RawImageGray8::runContrast()
{
	// Get the min/max values
	int minPix = 255,maxPix = -1;
	for (unsigned int ii=0;ii<totalSize();ii++)
	{
		unsigned char &val = img[ii];
		if (val < minPix)  minPix = val;
		if (val > maxPix)  maxPix = val;
	}
	
	// Now do the scale
	float scale = 256.0/(maxPix-minPix);
	for (unsigned int ii=0;ii<totalSize();ii++)
	{
		int newVal = (img[ii] - minPix)*scale;
		img[ii] = (newVal > 255) ? 255 : newVal;
	}
}

// Print the data around a cell, for debugging
void RawImageGray8::printCell(const char *what,int cx,int cy)
{
	printf("%s at (%d,%d)\n",what,cx,cy);
	for (unsigned int iy=cy+1;iy>=cy-1;iy--)
	{
		printf("  ");
		for (unsigned int ix=cx-1;ix<=cx+1;ix++)
		{
			printf("%4d\t",getPixel(ix, iy));
		}
		printf("\n");
	}
}

// Construct with a new image
RawImageGray32::RawImageGray32(int sizeX,int sizeY)
{
	this->sizeX = sizeX;  this->sizeY = sizeY;
	img = new int [sizeX*sizeY];
	bzero(img,totalSize()*sizeof(int));
}

// Destructor
RawImageGray32::~RawImageGray32()
{
	delete [] img;
	img = NULL;
}

// Construct a GDImage from the data
gdImagePtr RawImageGray32::makeImage(bool zeroAlpha)
{
	// Get the min and max
	int minPix=1<29,maxPix=-(1<<29);
	for (unsigned int ii=0;ii<totalSize();ii++)
	{
		int pix = img[ii];
		if (pix < minPix)  minPix = pix;
		if (pix > maxPix)  maxPix = pix;
	}
	// Floor at zero
	if (minPix > 0)  minPix = 0;  

	// Output image in grayscale
	gdImagePtr outImg = gdImageCreate(sizeX,sizeY);
	for (unsigned int ic=0;ic<255;ic++)
	  gdImageColorAllocate(outImg,ic,ic,ic);

	for (unsigned int ix=0;ix<sizeX;ix++)
	  for (unsigned int iy=0;iy<sizeY;iy++)
	    {
	      int pix = getPixel(ix,iy);
	      int scalePix = 255*(pix-minPix)/(float)(maxPix-minPix);
	      gdImageSetPixel(outImg,ix,iy,scalePix);
	    }

	return outImg;
}

// Print the data around a cell, for debugging
void RawImageGray32::printCell(const char *what,int cx,int cy)
{
	printf("%s at (%d,%d)\n",what,cx,cy);
	for (unsigned int iy=cy+1;iy>=cy-1;iy--)
	{
		printf("  ");
		for (unsigned int ix=cx-1;ix<=cx+1;ix++)
		{
			printf("%4d",getPixel(ix, iy));
		}
		printf("\n");
	}
}
