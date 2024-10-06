#include <gd.h>
#include <iostream>
#include <cstdlib>
#include "RawImage.h"

int main(int argc, char* argv[]) {
    // Ensure the correct number of arguments are provided
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input_image.png> <output_image.png>" << std::endl;
        return EXIT_FAILURE;
    }

    // Input and output file paths
    const char* inputFilePath = argv[1];
    const char* outputFilePath = argv[2];

    // Open the input PNG image using GD library
    FILE* inputFile = fopen(inputFilePath, "rb");
    if (!inputFile) {
        std::cerr << "Error: Could not open input file " << inputFilePath << std::endl;
        return EXIT_FAILURE;
    }

    // Load the image
    gdImagePtr inputImage = gdImageCreateFromPng(inputFile);
    fclose(inputFile);

    if (!inputImage) {
        std::cerr << "Error: Could not load image from " << inputFilePath << std::endl;
        return EXIT_FAILURE;
    }

    // Get the size of the image
    int width = gdImageSX(inputImage);
    int height = gdImageSY(inputImage);

    // Create a RawImageGray8 object with the image dimensions
    RawImageGray8 rawImage(width, height);

    // Copy the GD image into the RawImageGray8 object
    rawImage.copyFromGDImage(inputImage);

    // Apply contrast adjustment
    rawImage.runContrast();

    // Convert the processed image back into a GD image
    gdImagePtr outputImage = rawImage.makeGDImage();

    // Save the processed image to the output file
    FILE* outputFile = fopen(outputFilePath, "wb");
    if (!outputFile) {
        std::cerr << "Error: Could not open output file " << outputFilePath << std::endl;
        gdImageDestroy(inputImage);
        gdImageDestroy(outputImage);
        return EXIT_FAILURE;
    }

    gdImagePng(outputImage, outputFile);
    fclose(outputFile);

    // Clean up
    gdImageDestroy(inputImage);
    gdImageDestroy(outputImage);

    std::cout << "Processed image saved to " << outputFilePath << std::endl;
    return EXIT_SUCCESS;
}
