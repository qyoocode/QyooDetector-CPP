#include <iostream>
#include <string>
#include <gd.h>
#include "FeatureDetector.h"
#include "QyooModel.h"

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

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <image_file>" << std::endl;
        return 1;
    }

    std::string image_file = argv[1];

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

    std::cout << "Loaded image with size: " << processSizeX << "x" << processSizeY << std::endl;

    // Instantiate the FeatureProcessor with the image and its size
    FeatureProcessor* proc = new FeatureProcessor(theImage, processSizeX, processSizeY);
    proc->processImage();

    // Try to find the qyoo in the image
    if (proc->findQyoo() > 0) {
        // Process the dots for the qyoo found
        proc->findDots(theImage);

        // Get the first feature's dots processor
        FeatureDotsProcessor* closeupProc = proc->featureDots[0];

        // Use the QyooModel to verify and get the qyoo code
        QyooModel* qyooModel = QyooModel::getQyooModel();
        if (qyooModel->verifyCode(closeupProc->feat->dotBits)) {
            // Get the decimal string version of the qyoo code
            std::string foundQyooCode = qyooModel->decimalCodeStr(closeupProc->feat->dotBits);

            std::cout << "Found Qyoo Code: " << foundQyooCode << std::endl;
        } else {
            std::cerr << "Found Qyoo outline, but the code is invalid." << std::endl;
        }
    } else {
        std::cerr << "No Qyoo found in the image." << std::endl;
    }


    // Clean up
    gdImageDestroy(theImage); // Destroy the image to avoid memory leaks
    delete proc;

    return 0;
}
