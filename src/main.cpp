#include <iostream>
#include <string>
#include <gd.h>
#include "FeatureDetector.h"

// Global verbose flag for controlling debug output
bool verbose = false;

// Function to load a PNG image using gd
gdImagePtr loadImage(const std::string& fileName) {
    FILE *infile = fopen(fileName.c_str(), "rb");
    if (!infile) {
        std::cerr << "Error: Unable to open image file: " << fileName << std::endl;
        return nullptr;
    }

    gdImagePtr img = gdImageCreateFromPng(infile);
    fclose(infile);

    if (!img) {
        std::cerr << "Error: Unable to load image: " << fileName << std::endl;
    }

    return img;
}

// Function to handle verbose logging
void logVerbose(const std::string& message) {
    if (verbose) {
        std::cout << "Debug: " << message << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <image_file> [--v|--verbose]" << std::endl;
        return 1;
    }

    // Check if verbose flag is set
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--v" || arg == "--verbose") {
            verbose = true;  // Enable verbose logging
        }
    }

    std::string image_file = argv[1]; // The first argument should be the image file

    // Load the image (using gdImagePtr)
    gdImagePtr theImage = loadImage(image_file);
    if (!theImage) {
        return 1; // Exit if image loading fails
    }

    // Convert palette-based image to true color if necessary
    if (!gdImageTrueColor(theImage)) {
        gdImagePtr trueColorImg = gdImageCreateTrueColor(gdImageSX(theImage), gdImageSY(theImage));
        if (!trueColorImg) {
            std::cerr << "Error: Unable to create true color image." << std::endl;
            gdImageDestroy(theImage);
            return 1;
        }

        // Copy the palette-based image into the true color image
        gdImageCopy(trueColorImg, theImage, 0, 0, 0, 0, gdImageSX(theImage), gdImageSY(theImage));
        gdImageDestroy(theImage);  // Clean up the original palette-based image
        theImage = trueColorImg;   // Replace with the true-color image
    }

    // Get image size from the loaded image
    int processSizeX = gdImageSX(theImage); // Width
    int processSizeY = gdImageSY(theImage); // Height

    logVerbose("Loaded image with size: " + std::to_string(processSizeX) + "x" + std::to_string(processSizeY) );

    // Instantiate the FeatureProcessor with the image and its size
    FeatureProcessor* proc = new FeatureProcessor(theImage, processSizeX, processSizeY);
    proc->processImage();

    // Try to find the qyoo in the image
    if (proc->findQyoo() > 0) {
        // Process the dots for the qyoo found
        proc->findDots(theImage);

        logVerbose("Feature processing completed successfully.");

    } else {
        std::cerr << "No Qyoo found in the image." << std::endl;
    }

    // Clean up
    gdImageDestroy(theImage); // Destroy the image to avoid memory leaks
    delete proc;

    return 0;
}