/*
 *  RawImage.h
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/15/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#ifndef RAWIMAGE_H
#define RAWIMAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gd.h>
#include <cml/cml.h>

// Typedef for a 3x3 matrix using row_basis layout for transformations.
typedef cml::matrix<double, cml::fixed<3, 3>, cml::row_basis> QyooMatrix;

/**
 * Debug routine to save an image as a PNG file.
 * @param theImage The GD image to be saved.
 * @param fileName The file path for the output PNG image.
 */
void gdSaveAsPng(gdImagePtr theImage, const char *fileName);

/**
 * Flip a GD image both horizontally and vertically and return the flipped version.
 * @param theImage The input GD image.
 * @return A new GD image with the flipped content.
 */
gdImagePtr gdFlipImage(gdImagePtr theImage);

/**
 * A class that wraps raw 8-bit grayscale image data.
 */
class RawImageGray8
{
public:
    /**
     * Construct a RawImageGray8 using raw image data.
     * @param imgData The raw image data.
     * @param sizeX The width of the image.
     * @param sizeY The height of the image.
     * @param isMine If true, RawImage is responsible for managing the memory.
     * @param useFree If true, memory should be freed using `free` (otherwise `delete[]` is used).
     */
    RawImageGray8(void *imgData, int sizeX, int sizeY, bool isMine = true, bool useFree = false);

    /**
     * Allocate a blank image with the given size.
     * @param sizeX The width of the image.
     * @param sizeY The height of the image.
     */
    RawImageGray8(int sizeX, int sizeY);

    /**
     * Destructor to free the allocated memory.
     */
    ~RawImageGray8();

    /**
     * Get the width of the image.
     * @return The image width.
     */
    inline int getSizeX() { return sizeX; }

    /**
     * Get the height of the image.
     * @return The image height.
     */
    inline int getSizeY() { return sizeY; }

    /**
     * Get the total number of pixels in the image.
     * @return The total image size.
     */
    inline int totalSize() { return sizeX * sizeY; }

    /**
     * Get a reference to a specific pixel in the image.
     * @param pixX The x-coordinate of the pixel.
     * @param pixY The y-coordinate of the pixel.
     * @return A reference to the pixel value.
     */
    inline unsigned char &getPixel(int pixX, int pixY) { return img[pixY * sizeX + pixX]; }

    /**
     * Get a pointer to the raw image data.
     * @return A pointer to the raw image data.
     */
    inline unsigned char *getImgData() { return img; }

    /**
     * Copy data from a GD image into this grayscale image.
     * @param inImage The source GD image.
     */
    void copyFromGDImage(gdImagePtr inImage);

    /**
     * Copy data from a GD image using a transformation matrix.
     * @param inImage The source GD image.
     * @param mat The transformation matrix to apply.
     */
    void copyFromGDImage(gdImagePtr inImage, QyooMatrix &mat);

    /**
     * Apply a simple contrast scaling operation to the image.
     */
    void runContrast();

    /**
     * Convert the grayscale image data into a GD image.
     * @return A GD image representing the grayscale data.
     */
    gdImagePtr makeGDImage();

    /**
     * Print the pixel values around a specific cell for debugging purposes.
     * @param what The label for the printed data.
     * @param cx The x-coordinate of the center cell.
     * @param cy The y-coordinate of the center cell.
     */
    void printCell(const char *what, int cx, int cy);

protected:
    bool isMine;      ///< Indicates if the RawImage class owns the image memory.
    bool useFree;     ///< Indicates whether to use `free` or `delete[]` for memory deallocation.
    int sizeX, sizeY; ///< Dimensions of the image.
    unsigned char *img; ///< Pointer to the raw image data.
};

/**
 * A class that wraps raw 32-bit grayscale image data.
 */
class RawImageGray32
{
public:
    /**
     * Allocate a blank image with the given size.
     * @param sizeX The width of the image.
     * @param sizeY The height of the image.
     */
    RawImageGray32(int sizeX, int sizeY);

    /**
     * Destructor to free the allocated memory.
     */
    ~RawImageGray32();

    /**
     * Get the width of the image.
     * @return The image width.
     */
    inline int getSizeX() { return sizeX; }

    /**
     * Get the height of the image.
     * @return The image height.
     */
    inline int getSizeY() { return sizeY; }

    /**
     * Get the total number of pixels in the image.
     * @return The total image size.
     */
    inline int totalSize() { return sizeX * sizeY; }

    /**
     * Get a reference to a specific pixel in the image.
     * @param pixX The x-coordinate of the pixel.
     * @param pixY The y-coordinate of the pixel.
     * @return A reference to the pixel value.
     */
    inline int &getPixel(int pixX, int pixY) { return img[pixY * sizeX + pixX]; }

    /**
     * Get a pointer to the raw image data.
     * @return A pointer to the raw image data.
     */
    inline int *getImgData() { return img; }

    /**
     * Convert the grayscale image data into a GD image.
     * @param zeroAlpha If true, set alpha to zero.
     * @return A GD image representing the grayscale data.
     */
    gdImagePtr makeImage(bool zeroAlpha = false);

    /**
     * Print the pixel values around a specific cell for debugging purposes.
     * @param what The label for the printed data.
     * @param cx The x-coordinate of the center cell.
     * @param cy The y-coordinate of the center cell.
     */
    void printCell(const char *what, int cx, int cy);

protected:
    int sizeX, sizeY; ///< Dimensions of the image.
    int *img; ///< Pointer to the raw image data.
};

/**
 * Helper function to release a buffer.
 * @param info The info parameter for the buffer release.
 * @param data The data buffer to be released.
 * @param size The size of the data buffer.
 */
void releaseBuffer(void *info, const void *data, size_t size);

#endif // RAWIMAGE_H
