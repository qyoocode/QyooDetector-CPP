/*
 *  RawImage.cpp
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/15/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#include "RawImage.h"

/**
 * Save a GD image as a PNG file.
 * @param theImage The GD image pointer to save.
 * @param fileName The output file name for the PNG image.
 */
void gdSaveAsPng(gdImagePtr theImage, const char *fileName)
{
    FILE *fp = fopen(fileName, "wb");
    if (!fp) return;

    gdImagePng(theImage, fp);
    fclose(fp);
}

/**
 * Flip a GD image horizontally and vertically and return the flipped version.
 * @param theImage The input GD image pointer.
 * @return A new GD image pointer representing the flipped image.
 */
gdImagePtr gdFlipImage(gdImagePtr theImage)
{
    gdImagePtr outImg = gdImageCreate(gdImageSY(theImage), gdImageSX(theImage));
    for (unsigned int ic = 0; ic < 255; ic++)
        gdImageColorAllocate(outImg, ic, ic, ic);

    for (unsigned int ix = 0; ix < gdImageSX(theImage); ix++)
        for (unsigned int iy = 0; iy < gdImageSY(theImage); iy++)
        {
            int pixVal = gdImageRed(theImage, gdImageGetPixel(theImage, ix, iy));
            int red = gdImageRed(theImage, pixVal);
            int green = gdImageGreen(theImage, pixVal);
            int blue = gdImageBlue(theImage, pixVal);
            int outVal = (red + green + blue) / 3;
            gdImageSetPixel(outImg, iy, ix, outVal);
        }

    return outImg;
}

/**
 * Constructor for creating a grayscale image of 8-bit depth.
 * @param sizeX The width of the image.
 * @param sizeY The height of the image.
 */
RawImageGray8::RawImageGray8(int sizeX, int sizeY)
{
    this->sizeX = sizeX;
    this->sizeY = sizeY;
    isMine = true;
    useFree = false;
    img = new unsigned char[sizeX * sizeY];
    memset(img, 0, totalSize());
}

/**
 * Destructor for the 8-bit grayscale image.
 * Frees up the allocated memory for the image data.
 */
RawImageGray8::~RawImageGray8()
{
    if (isMine)
    {
        if (useFree)
            free(img);
        else
            delete[] img;
    }
    img = NULL;
}

/**
 * Copy pixel data from a GD image into the internal grayscale image.
 * @param inImage The input GD image pointer.
 */
void RawImageGray8::copyFromGDImage(gdImagePtr inImage)
{
    gdImagePtr tmpImg = gdImageCreate(sizeX, sizeY);
    for (unsigned int ic = 0; ic < 255; ic++)
        gdImageColorAllocate(tmpImg, ic, ic, ic);
    gdImageCopyResampled(tmpImg, inImage, 0, 0, 0, 0, sizeX, sizeY, gdImageSX(inImage), gdImageSY(inImage));

    // Extract grayscale pixel data
    for (unsigned ix = 0; ix < tmpImg->sx; ix++)
        for (unsigned int iy = 0; iy < tmpImg->sy; iy++)
        {
            int pixVal = gdImageGetPixel(tmpImg, ix, iy);
            int red = gdImageRed(tmpImg, pixVal);
            int green = gdImageGreen(tmpImg, pixVal);
            int blue = gdImageGreen(tmpImg, pixVal);
            int gray = (red + green + blue) / 3;
            getPixel(ix, iy) = gdImageRed(tmpImg, gray);
        }

    gdImageDestroy(tmpImg);
}

/**
 * Copy pixel data from a GD image using a transformation matrix.
 * @param inImage The input GD image pointer.
 * @param mat The transformation matrix to apply when copying the image.
 */
void RawImageGray8::copyFromGDImage(gdImagePtr inImage, QyooMatrix &mat)
{
    QyooMatrix invMat = mat;
    invMat.inverse();

    for (unsigned int ix = 0; ix < sizeX; ix++)
        for (unsigned int iy = 0; iy < sizeY; iy++)
        {
            float ixP = (float)ix / (float)sizeX;
            float iyP = (float)iy / (float)sizeY;
            cml::vector3d pt = invMat * cml::vector3d(ixP, iyP, 1.0);
            int destX = pt[0] + 0.5, destY = pt[1] + 0.5;

            if (destX < 0) destX = 0;
            if (destX >= gdImageSX(inImage)) destX = gdImageSX(inImage) - 1;
            if (destY < 0) destY = 0;
            if (destY >= gdImageSY(inImage)) destY = gdImageSY(inImage) - 1;

            int pixVal = gdImageGetPixel(inImage, destX, destY);
            getPixel(ix, iy) = gdImageRed(inImage, pixVal);
        }
}

/**
 * Create a GD image from the internal grayscale image data.
 * @return A GD image pointer representing the grayscale image.
 */
gdImagePtr RawImageGray8::makeGDImage()
{
    gdImagePtr newImage = gdImageCreate(sizeX, sizeY);
    for (unsigned int ic = 0; ic < 255; ic++)
        gdImageColorAllocate(newImage, ic, ic, ic);

    for (unsigned int ix = 0; ix < sizeX; ix++)
        for (unsigned int iy = 0; iy < sizeY; iy++)
        {
            gdImageSetPixel(newImage, ix, iy, getPixel(ix, iy));
        }

    return newImage;
}

/**
 * Perform a simple contrast scaling operation on the grayscale image.
 * Adjusts pixel values to enhance contrast.
 */
void RawImageGray8::runContrast()
{
    int minPix = 255, maxPix = -1;
    for (unsigned int ii = 0; ii < totalSize(); ii++)
    {
        unsigned char &val = img[ii];
        if (val < minPix) minPix = val;
        if (val > maxPix) maxPix = val;
    }

    float scale = 256.0 / (maxPix - minPix);
    for (unsigned int ii = 0; ii < totalSize(); ii++)
    {
        int newVal = (img[ii] - minPix) * scale;
        img[ii] = (newVal > 255) ? 255 : newVal;
    }
}

/**
 * Print the pixel data around a specific cell for debugging purposes.
 * @param what The message to display.
 * @param cx The x-coordinate of the cell.
 * @param cy The y-coordinate of the cell.
 */
void RawImageGray8::printCell(const char *what, int cx, int cy)
{
    printf("%s at (%d,%d)\n", what, cx, cy);
    for (unsigned int iy = cy + 1; iy >= cy - 1; iy--)
    {
        printf("  ");
        for (unsigned int ix = cx - 1; ix <= cx + 1; ix++)
        {
            printf("%4d\t", getPixel(ix, iy));
        }
        printf("\n");
    }
}

/**
 * Constructor for creating a grayscale image of 32-bit depth.
 * @param sizeX The width of the image.
 * @param sizeY The height of the image.
 */
RawImageGray32::RawImageGray32(int sizeX, int sizeY)
{
    this->sizeX = sizeX;
    this->sizeY = sizeY;
    img = new int[sizeX * sizeY];
    bzero(img, totalSize() * sizeof(int));
}

/**
 * Destructor for the 32-bit grayscale image.
 * Frees up the allocated memory for the image data.
 */
RawImageGray32::~RawImageGray32()
{
    delete[] img;
    img = NULL;
}

/**
 * Create a GD image from the internal 32-bit grayscale image data.
 * @param zeroAlpha Boolean flag to set whether alpha is zero.
 * @return A GD image pointer representing the grayscale image.
 */
gdImagePtr RawImageGray32::makeImage(bool zeroAlpha)
{
    int minPix = 1 << 29, maxPix = -(1 << 29);
    for (unsigned int ii = 0; ii < totalSize(); ii++)
    {
        int pix = img[ii];
        if (pix < minPix) minPix = pix;
        if (pix > maxPix) maxPix = pix;
    }
    if (minPix > 0) minPix = 0;

    gdImagePtr outImg = gdImageCreate(sizeX, sizeY);
    for (unsigned int ic = 0; ic < 255; ic++)
        gdImageColorAllocate(outImg, ic, ic, ic);

    for (unsigned int ix = 0; ix < sizeX; ix++)
        for (unsigned int iy = 0; iy < sizeY; iy++)
        {
            int pix = getPixel(ix, iy);
            int scalePix = 255 * (pix - minPix) / (float)(maxPix - minPix);
            gdImageSetPixel(outImg, ix, iy, scalePix);
        }

    return outImg;
}

/**
 * Print the pixel data around a specific cell for debugging purposes.
 * @param what The message to display.
 * @param cx The x-coordinate of the cell.
 * @param cy The y-coordinate of the cell.
 */
void RawImageGray32::printCell(const char *what, int cx, int cy)
{
    printf("%s at (%d,%d)\n", what, cx, cy);
    for (unsigned int iy = cy + 1; iy >= cy - 1; iy--)
    {
        printf("  ");
        for (unsigned int ix = cx - 1; ix <= cx + 1; ix++)
        {
            printf("%4d", getPixel(ix, iy));
        }
        printf("\n");
    }
}
