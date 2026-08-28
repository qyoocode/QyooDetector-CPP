#include "ImageLoader.h"

#include <cstring>
#include <iostream>


gdImagePtr loadImage(const std::string &fileName)
{
    FILE *input = fopen(fileName.c_str(), "rb");
    if (input == nullptr) {
        std::cerr << "Error: Unable to open image file: " << fileName << std::endl;
        return nullptr;
    }

    unsigned char signature[8] = {};
    size_t signatureSize = fread(signature, 1, sizeof(signature), input);
    rewind(input);

    static const unsigned char pngSignature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    bool isPng = signatureSize == sizeof(signature) &&
                 std::memcmp(signature, pngSignature, sizeof(signature)) == 0;
    bool isJpeg = signatureSize >= 3 && signature[0] == 0xff &&
                  signature[1] == 0xd8 && signature[2] == 0xff;

    gdImagePtr image = nullptr;
    const char *format = nullptr;
    if (isPng) {
        format = "PNG";
        image = gdImageCreateFromPng(input);
    } else if (isJpeg) {
        format = "JPEG";
        image = gdImageCreateFromJpeg(input);
    } else {
        std::cerr << "Error: Unsupported image format (expected PNG or JPEG): "
                  << fileName << std::endl;
    }
    fclose(input);

    if (format != nullptr && image == nullptr)
        std::cerr << "Error: Unable to decode " << format << " image: " << fileName << std::endl;
    return image;
}
