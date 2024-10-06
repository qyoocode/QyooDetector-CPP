/*
 *  RawImage.h
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/15/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef QYOO_CMD
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#else
#include <gd.h>
#import <cml/cml.h>
#endif

#ifdef QYOO_CMD
// Debug routine to save out an image
void gdSaveAsPng(gdImagePtr theImage,const char *fileName);

// Return a version of the image flipped
gdImagePtr gdFlipImage(gdImagePtr theImage);

typedef cml::matrix<double, cml::fixed<3,3>, cml::row_major> QyooMatrix;
#else
typedef CGAffineTransform QyooMatrix;
#endif

/* A raw image wrapper for 8 bit grayscale data
 */
class RawImageGray8
{
public:
	// Pass in raw image data, if true RawImage is responsible for deletion
	// Set useFree to true if malloc was used to create the data
	RawImageGray8(void *imgData,int sizeX,int sizeY,bool isMine=true,bool useFree=false);
	// Just allocate the image
	RawImageGray8(int sizeX,int sizeY);
	~RawImageGray8();
	
	// Sizes
	inline int getSizeX() { return sizeX; }
	inline int getSizeY() { return sizeY; }
	inline int totalSize() { return sizeX*sizeY; }
	
	// Return a pixel reference
	inline unsigned char &getPixel(int pixX,int pixY) { return img[pixY*sizeX + pixX]; }
	
	// Reference to the whole thing
	inline unsigned char *getImgData() { return img; }
	
	// Render from a UIImage
	// Do a dirty contrast pass, optionally
#ifdef QYOO_CMD
	void copyFromGDImage(gdImagePtr inImage);
#else
	void renderFromImage(UIImage *inImage,BOOL flip,BOOL vertFlip);
#endif
	
	// Render from a UIImage, but use a transform
#ifdef QYOO_CMD
	void copyFromGDImage(gdImagePtr inImage,QyooMatrix &mat);
#else
	void renderFromImage(UIImage *inImage,CGAffineTransform &mat,BOOL flip,BOOL vertFlip);
#endif
		
	// Do a quick and dirty contrast scale
	void runContrast();
	
#ifdef QYOO_CMD
	// Return a brand new GD Image from this data
	gdImagePtr makeGDImage();
#else
	// Make a UIImage out of this
	// If makeCopy is true, we'll copy the raw data
	// If you don't do that, you have to keep this object around
	UIImage *makeImage(bool makeCopy=true);
	
	// Make a UIImage, but index the colors
	UIImage *makeIndexImage(unsigned char check=0xff,unsigned char mask=0xff);
#endif
	
	// Print data around a cell, for debugging
	void printCell(const char *what,int cx,int cy);
protected:
	bool isMine;      // Set if we need to delete it
	bool useFree;     // How to delete it
	int sizeX,sizeY;  // Image size
	unsigned char *img;
};

/* 32 bit Raw Image
 */
class RawImageGray32
{
	public:
	// Just allocate the image
	RawImageGray32(int sizeX,int sizeY);
	~RawImageGray32();
	
	// Sizes
	inline int getSizeX() { return sizeX; }
	inline int getSizeY() { return sizeY; }
	inline int totalSize() { return sizeX*sizeY; }
	
	// Return a pixel reference
	inline int &getPixel(int pixX,int pixY) { return img[pixY*sizeX + pixX]; }
	
	// Reference to the whole thing
	inline int *getImgData() { return img; }
	
#ifdef QYOO_CMD
	gdImagePtr makeImage(bool zeroAlpha=false);
#else
	// Make a UIImage out of this
	// We'll scale accordingly
	UIImage *makeImage(bool zeroAlpha=false);
	
	// Make a UIImage, but index the colors
	UIImage *makeIndexImage(unsigned int check=0xffff,unsigned int mask=0xffff);
#endif
	
	// Print data around a cell, for debugging
	void printCell(const char *what,int cx,int cy);
protected:
		int sizeX,sizeY;  // Image size
		int *img;
};

#ifndef QYOO_CMD
// Construct from a UIImage (returned by the camera, probably)
// Pass in the raw image so we can reuse it
void RenderToRawImage(UIImage *inImg,RawImageGray8 *outImg);
#endif

void releaseBuffer(void *info, const void *data, size_t size);
