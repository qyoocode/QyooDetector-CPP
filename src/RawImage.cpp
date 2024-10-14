/*
 *  RawImage.cpp
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/15/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#include "RawImage.h"
#include "Logger.h"

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
 * Rotate the RawImageGray8 by a specified angle using GD.
 * @param angle The angle in degrees to rotate the image.
 * @return A new RawImageGray8 image pointer representing the rotated image.
 */
RawImageGray8* RawImageGray8::rotateImage(float angle)
{
    // Create a GD image from RawImageGray8 data
    gdImagePtr gdImage = this->makeGDImage();
    if (!gdImage) {
        logVerbose("Failed to create GD image for rotation.");
        return nullptr;  // Handle failure
    }

    // Rotate the image using GD's rotate function with nearest-neighbor interpolation
    gdImagePtr rotatedImage = gdImageRotateInterpolated(gdImage, angle, 0);  // 0 for black background color
    if (!rotatedImage) {
        gdImageDestroy(gdImage);  // Clean up the original GD image
        logVerbose("Failed to rotate GD image.");
        return nullptr;  // Handle failure
    }

    // Create a new RawImageGray8 to hold the rotated result
    RawImageGray8* rotatedRawImg = new RawImageGray8(rotatedImage->sx, rotatedImage->sy);

    // Copy the rotated GD image data back to the new RawImageGray8 object
    rotatedRawImg->copyFromGDImage(rotatedImage);

    // Clean up the GD images
    gdImageDestroy(gdImage);      // Clean up the original GD image
    gdImageDestroy(rotatedImage); // Clean up the rotated GD image

    return rotatedRawImg;
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


/**
 * Rotate the RawImageGray8 by a specified angle using GD.
 * @param angle The angle in degrees to rotate the image.
 * @return A new RawImageGray8 image pointer representing the rotated image.
 */
 RawImageGray32* RawImageGray32::rotateImage(float angle)
 {
     // Create a GD image from RawImageGray32 data
     gdImagePtr gdImage = this->makeGDImage();
     if (!gdImage) {
         logVerbose("Failed to create GD image for rotation.");
         return nullptr;  // Handle failure
     }

     // Rotate the image using GD's rotate function with nearest-neighbor interpolation
     gdImagePtr rotatedImage = gdImageRotateInterpolated(gdImage, angle, 0);  // 0 for black background color
     if (!rotatedImage) {
         gdImageDestroy(gdImage);  // Clean up the original GD image
         gdImage = nullptr;        // Avoid double free
         logVerbose("Failed to rotate GD image.");
         return nullptr;  // Handle failure
     }

     // Create a new RawImageGray32 to hold the rotated result
     RawImageGray32* rotatedRawImg = new RawImageGray32(rotatedImage->sx, rotatedImage->sy);

     // Copy the rotated GD image data back to the new RawImageGray32 object
     rotatedRawImg->copyFromGDImage(rotatedImage);

     // Clean up the GD images
     if (gdImage) {
         gdImageDestroy(gdImage);  // Clean up the original GD image
         gdImage = nullptr;        // Set to nullptr to avoid double free
     }

     if (rotatedImage) {
         gdImageDestroy(rotatedImage); // Clean up the rotated GD image
         rotatedImage = nullptr;       // Set to nullptr to avoid double free
     }

     return rotatedRawImg;
 }


/**
 * Create a GD image from the internal 32-bit grayscale image data.
 * @return A GD image pointer representing the 32-bit grayscale image.
 */
gdImagePtr RawImageGray32::makeGDImage()
{
    gdImagePtr newImage = gdImageCreate(sizeX, sizeY);
    for (unsigned int ic = 0; ic < 255; ic++)
        gdImageColorAllocate(newImage, ic, ic, ic);

    // Scale the 32-bit image data to 8-bit for GD
    int minPix = INT_MAX, maxPix = INT_MIN;
    for (unsigned int i = 0; i < sizeX * sizeY; i++)
    {
        if (img[i] < minPix) minPix = img[i];
        if (img[i] > maxPix) maxPix = img[i];
    }

    for (unsigned int ix = 0; ix < sizeX; ix++)
    {
        for (unsigned int iy = 0; iy < sizeY; iy++)
        {
            int pix = img[ix + iy * sizeX];
            int scalePix = 255 * (pix - minPix) / (maxPix - minPix);
            gdImageSetPixel(newImage, ix, iy, scalePix);
        }
    }

    return newImage;
}

/**
 * Copy pixel data from a GD image into the internal 32-bit grayscale image.
 * @param inImage The input GD image pointer.
 */
void RawImageGray32::copyFromGDImage(gdImagePtr inImage)
{
    // Resize if necessary
    if (sizeX != gdImageSX(inImage) || sizeY != gdImageSY(inImage))
    {
        delete[] img;
        sizeX = gdImageSX(inImage);
        sizeY = gdImageSY(inImage);
        img = new int[sizeX * sizeY];
    }

    // Extract pixel data from the GD image and store it in 32-bit format
    for (unsigned int ix = 0; ix < sizeX; ix++)
    {
        for (unsigned int iy = 0; iy < sizeY; iy++)
        {
            int pixVal = gdImageGetPixel(inImage, ix, iy);
            int red = gdImageRed(inImage, pixVal);
            int green = gdImageGreen(inImage, pixVal);
            int blue = gdImageBlue(inImage, pixVal);
            img[ix + iy * sizeX] = (red + green + blue) / 3;  // Convert to grayscale
        }
    }
}

gdImagePtr RawImageGray32::makeGDImageTrue()
{
    // Convert RawImageGray32 to a GD true-color image
    gdImagePtr newImage = gdImageCreateTrueColor(sizeX, sizeY);  // Use gdImageCreateTrueColor for true color image

    if (!newImage) {
        logVerbose("Failed to create GD true-color image.");
        return nullptr;
    }

    for (unsigned int ix = 0; ix < sizeX; ix++)
    {
        for (unsigned int iy = 0; iy < sizeY; iy++)
        {
            int pix = getPixel(ix, iy);  // Assuming getPixel returns grayscale pixel value
            int color = gdTrueColor(pix, pix, pix);  // Set R, G, B to the same grayscale value
            gdImageSetPixel(newImage, ix, iy, color);  // Use true color
        }
    }

    return newImage;
}

gdImagePtr RawImageGray8::makeGDImageTrue()
{
    // Convert RawImageGray32 to a GD true-color image
    gdImagePtr newImage = gdImageCreateTrueColor(sizeX, sizeY);  // Use gdImageCreateTrueColor for true color image

    if (!newImage) {
        logVerbose("Failed to create GD true-color image.");
        return nullptr;
    }

    for (unsigned int ix = 0; ix < sizeX; ix++)
    {
        for (unsigned int iy = 0; iy < sizeY; iy++)
        {
            int pix = getPixel(ix, iy);  // Assuming getPixel returns grayscale pixel value
            int color = gdTrueColor(pix, pix, pix);  // Set R, G, B to the same grayscale value
            gdImageSetPixel(newImage, ix, iy, color);  // Use true color
        }
    }

    return newImage;
}
