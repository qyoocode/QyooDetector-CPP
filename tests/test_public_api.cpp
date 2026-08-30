#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <gd.h>

#include "qyoo_detector.h"

namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(1);
    }
}

struct Pixels
{
    uint32_t width;
    uint32_t height;
    size_t stride;
    std::vector<uint8_t> bytes;
};

Pixels loadJpegRgb(const char *path)
{
    FILE *input = std::fopen(path, "rb");
    require(input != nullptr, std::string("cannot open valid fixture: ") + path);
    gdImagePtr image = gdImageCreateFromJpeg(input);
    std::fclose(input);
    require(image != nullptr, "cannot decode valid JPEG fixture");

    Pixels pixels;
    pixels.width = static_cast<uint32_t>(gdImageSX(image));
    pixels.height = static_cast<uint32_t>(gdImageSY(image));
    const size_t rowBytes = static_cast<size_t>(pixels.width) * 3;
    pixels.stride = rowBytes + 13;
    pixels.bytes.assign(pixels.stride * pixels.height, 0xa5);
    for (uint32_t y = 0; y < pixels.height; ++y)
        for (uint32_t x = 0; x < pixels.width; ++x)
        {
            int source = gdImageGetPixel(image, static_cast<int>(x), static_cast<int>(y));
            uint8_t *destination = pixels.bytes.data() + y * pixels.stride + x * 3;
            destination[0] = static_cast<uint8_t>(gdImageRed(image, source));
            destination[1] = static_cast<uint8_t>(gdImageGreen(image, source));
            destination[2] = static_cast<uint8_t>(gdImageBlue(image, source));
        }
    gdImageDestroy(image);
    return pixels;
}

qyoo_image_t imageView(const Pixels &pixels)
{
    qyoo_image_t image = {};
    image.struct_size = sizeof(image);
    image.width = pixels.width;
    image.height = pixels.height;
    image.stride_bytes = pixels.stride;
    image.pixel_format = QYOO_PIXEL_FORMAT_RGB24;
    image.pixels = pixels.bytes.data();
    image.buffer_size = pixels.bytes.size();
    return image;
}

void expectNoQyoo(qyoo_detector_t *detector, const qyoo_image_t &image)
{
    qyoo_result_array_t results = {};
    require(qyoo_detector_detect(detector, &image, &results) == QYOO_STATUS_OK,
            "valid blank image is processed successfully");
    require(results.outcome == QYOO_DETECTION_NO_QYOO && results.count == 0 &&
            results.results == nullptr,
            "blank image is an explicit no-Qyoo outcome");
    qyoo_result_array_release(&results);
}
}

int main(int argc, char **argv)
{
    require(argc == 2, "usage: test_public_api valid-qyoo.jpg");
    require(qyoo_detector_api_version() == QYOO_DETECTOR_API_VERSION,
            "runtime and header API versions agree");
    require(std::string(qyoo_status_string(QYOO_STATUS_OK)) == "ok",
            "status strings are available");

    qyoo_detector_t *first = nullptr;
    qyoo_detector_t *second = nullptr;
    require(qyoo_detector_create(nullptr) == QYOO_STATUS_INVALID_ARGUMENT,
            "null context output is rejected");
    require(qyoo_detector_create(&first) == QYOO_STATUS_OK && first != nullptr,
            "first context is created");
    require(qyoo_detector_create(&second) == QYOO_STATUS_OK && second != nullptr,
            "second independent context is created");

    std::vector<uint8_t> grayBytes((96 + 11) * 64, 255);
    qyoo_image_t gray = {};
    gray.struct_size = sizeof(gray);
    gray.width = 96;
    gray.height = 64;
    gray.stride_bytes = 107;
    gray.pixel_format = QYOO_PIXEL_FORMAT_GRAY8;
    gray.pixels = grayBytes.data();
    gray.buffer_size = grayBytes.size();
    for (int iteration = 0; iteration < 3; ++iteration)
        expectNoQyoo(first, gray);
    expectNoQyoo(second, gray);

    std::vector<uint8_t> rgbBytes((128 * 3 + 7) * 80, 255);
    qyoo_image_t rgbBlank = {};
    rgbBlank.struct_size = sizeof(rgbBlank);
    rgbBlank.width = 128;
    rgbBlank.height = 80;
    rgbBlank.stride_bytes = 128 * 3 + 7;
    rgbBlank.pixel_format = QYOO_PIXEL_FORMAT_BGR24;
    rgbBlank.pixels = rgbBytes.data();
    rgbBlank.buffer_size = rgbBytes.size();
    expectNoQyoo(second, rgbBlank);

    std::vector<uint8_t> fourChannelBytes((64 * 4 + 9) * 48, 255);
    qyoo_image_t fourChannelBlank = {};
    fourChannelBlank.struct_size = sizeof(fourChannelBlank);
    fourChannelBlank.width = 64;
    fourChannelBlank.height = 48;
    fourChannelBlank.stride_bytes = 64 * 4 + 9;
    fourChannelBlank.pixels = fourChannelBytes.data();
    fourChannelBlank.buffer_size = fourChannelBytes.size();
    fourChannelBlank.pixel_format = QYOO_PIXEL_FORMAT_RGBA32;
    expectNoQyoo(first, fourChannelBlank);
    fourChannelBlank.pixel_format = QYOO_PIXEL_FORMAT_BGRA32;
    expectNoQyoo(first, fourChannelBlank);

    qyoo_result_array_t malformedResults = {};
    require(qyoo_detector_detect(nullptr, &gray, &malformedResults) ==
                QYOO_STATUS_INVALID_ARGUMENT,
            "null context is rejected");
    require(qyoo_detector_detect(first, nullptr, &malformedResults) ==
                QYOO_STATUS_INVALID_ARGUMENT,
            "null image is rejected");
    require(qyoo_detector_detect(first, &gray, nullptr) == QYOO_STATUS_INVALID_ARGUMENT,
            "null results output is rejected");
    qyoo_image_t malformed = gray;
    malformed.struct_size = 0;
    require(qyoo_detector_detect(first, &malformed, &malformedResults) ==
                QYOO_STATUS_INVALID_ARGUMENT,
            "unknown image structure is rejected");
    malformed = gray;
    malformed.width = 0;
    require(qyoo_detector_detect(first, &malformed, &malformedResults) ==
                QYOO_STATUS_INVALID_ARGUMENT,
            "zero image dimensions are rejected");
    malformed = gray;
    malformed.width = 65536;
    malformed.height = 65536;
    require(qyoo_detector_detect(first, &malformed, &malformedResults) ==
                QYOO_STATUS_INVALID_ARGUMENT,
            "dimensions that overflow internal pixel counts are rejected");
    malformed = gray;
    malformed.stride_bytes = malformed.width - 1;
    require(qyoo_detector_detect(first, &malformed, &malformedResults) ==
                QYOO_STATUS_INVALID_ARGUMENT,
            "short stride is rejected");
    malformed = gray;
    malformed.buffer_size = 1;
    require(qyoo_detector_detect(first, &malformed, &malformedResults) ==
                QYOO_STATUS_INVALID_ARGUMENT,
            "short buffer is rejected");
    malformed = gray;
    malformed.pixel_format = static_cast<qyoo_pixel_format_t>(999);
    require(qyoo_detector_detect(first, &malformed, &malformedResults) ==
                QYOO_STATUS_INVALID_ARGUMENT,
            "unknown pixel format is rejected");

    Pixels validPixels = loadJpegRgb(argv[1]);
    qyoo_image_t validImage = imageView(validPixels);
    qyoo_result_array_t validResults = {};
    require(qyoo_detector_detect(first, &validImage, &validResults) == QYOO_STATUS_OK,
            "known Qyoo is processed successfully");
    require(validResults.outcome == QYOO_DETECTION_DECODED && validResults.count == 1,
            "known Qyoo returns one decoded result");
    const qyoo_result_t &decoded = validResults.results[0];
    require(decoded.status == QYOO_RESULT_DECODED && decoded.payload == 511305600ULL &&
            decoded.payload_bit_count == QYOO_PAYLOAD_BITS,
            "raw 36-bit historical payload is returned in uint64_t");
    require(decoded.bounds.min_x >= 0.0 && decoded.bounds.min_y >= 0.0 &&
            decoded.bounds.max_x <= validImage.width &&
            decoded.bounds.max_y <= validImage.height,
            "bounds use original-image coordinates");
    require(decoded.carrier_quadrilateral_valid == 1,
            "accepted production carrier has projective geometry");
    for (size_t corner = 0; corner < QYOO_CARRIER_CORNER_COUNT; ++corner)
        require(std::isfinite(decoded.carrier_quadrilateral[corner].x) &&
                std::isfinite(decoded.carrier_quadrilateral[corner].y),
                "carrier quadrilateral coordinates are finite");
    require(decoded.normalization_mode == QYOO_NORMALIZATION_CARRIER_TEMPLATE,
            "normalization mode is explicit");

    qyoo_result_array_release(&validResults);
    require(validResults.count == 0 && validResults.results == nullptr &&
            validResults.outcome == QYOO_DETECTION_NO_QYOO,
            "result release clears ownership state");
    qyoo_result_array_release(&validResults);
    qyoo_result_array_release(nullptr);
    qyoo_detector_destroy(second);
    qyoo_detector_destroy(first);
    qyoo_detector_destroy(nullptr);

    std::cout << "PASS: portable C API input, results, contexts, validation, and cleanup"
              << std::endl;
    return 0;
}
