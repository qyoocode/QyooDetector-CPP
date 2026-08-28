#include <iostream>

#include <unistd.h>

#include "src/ImageLoader.h"


static bool expectImage(const char *path)
{
    gdImagePtr image = loadImage(path);
    if (image == nullptr) {
        std::cerr << "FAIL: supported image did not load: " << path << std::endl;
        return false;
    }
    bool validDimensions = gdImageSX(image) == 512 && gdImageSY(image) == 512;
    if (!validDimensions)
        std::cerr << "FAIL: unexpected dimensions for " << path << std::endl;
    gdImageDestroy(image);
    return validDimensions;
}


static bool expectRejectedBytes(const unsigned char *bytes, size_t size)
{
    char path[] = "/tmp/qyoo-image-input-XXXXXX";
    int descriptor = mkstemp(path);
    if (descriptor < 0)
        return false;
    ssize_t written = write(descriptor, bytes, size);
    close(descriptor);
    gdImagePtr image = loadImage(path);
    unlink(path);
    if (image != nullptr)
        gdImageDestroy(image);
    return written == static_cast<ssize_t>(size) && image == nullptr;
}


int main(int argc, char **argv)
{
    if (argc != 4) {
        std::cerr << "usage: " << argv[0] << " perfect.png perfect.jpg perfect.jpeg" << std::endl;
        return 2;
    }
    bool success = expectImage(argv[1]) && expectImage(argv[2]) && expectImage(argv[3]);
    const unsigned char unsupported[] = {'n', 'o', 't', '-', 'a', 'n', '-', 'i', 'm', 'a', 'g', 'e'};
    const unsigned char truncatedJpeg[] = {0xff, 0xd8, 0xff, 0x00};
    if (!expectRejectedBytes(unsupported, sizeof(unsupported))) {
        std::cerr << "FAIL: unsupported bytes were not rejected" << std::endl;
        success = false;
    }
    if (!expectRejectedBytes(truncatedJpeg, sizeof(truncatedJpeg))) {
        std::cerr << "FAIL: truncated JPEG was not rejected" << std::endl;
        success = false;
    }
    if (!success)
        return 1;
    std::cout << "PASS: PNG, JPG, and JPEG inputs load; invalid inputs fail" << std::endl;
    return 0;
}
