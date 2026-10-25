#include <iostream>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
#include "src/FeatureDetector.h"
#include "qyoo_detector.h"

// Exercise the actual sampler after normalization. These patches model the
// reproduced failure: partial foreground distributed across individually clear
// cells. No camera photograph or historical campaign asset is published here.
static void check(bool white, int partialCells, bool populated, int templateForeground = 0)
{
    gdImagePtr image = gdImageCreateTrueColor(88, 88);
    if (!image) throw std::runtime_error("image allocation failed");
    Feature feature;
    feature.imgSizeX = feature.imgSizeY = 88;
    FeatureDotsProcessor dots(image, nullptr, &feature,
        NormalizationCarrierTemplate, "", QualifiedAffineFallback, false, false);
    gdImageDestroy(image);
    dots.normalizationAvailable = true;
    dots.carrierTemplateGridForegroundPixels = templateForeground;
    dots.carrierTemplatePayload = std::string(36, '0');
    for (int y = 0; y < 88; ++y)
        for (int x = 0; x < 88; ++x)
            dots.grayImg->getPixel(x, y) = white ? 255 : 0;
    for (int cell = 0; cell < partialCells; ++cell)
    {
        int x = 16 + (cell % 6) * 11, y = 16 + (cell / 6) * 11;
        for (int dx = -2; dx <= 2; ++dx)
            dots.grayImg->getPixel(x + dx, y) = white ? 0 : 255;
    }
    if (populated)
    {
        for (int dy = -4; dy <= 4; ++dy)
            for (int dx = -4; dx <= 4; ++dx)
                if (dx * dx + dy * dy < 25)
                    dots.grayImg->getPixel(16 + dx, 16 + dy) = white ? 0 : 255;
        dots.carrierTemplatePayload[35] = '1';
    }
    dots.findDotsGray();
    bool shouldReject = (partialCells > 0 || templateForeground > 0) && !populated;
    if (shouldReject)
    {
        if (!dots.qyooBits.empty() || !feature.dotBinStr.empty() ||
            !feature.dotDecStr.empty() || feature.projectivePayloadExtracted)
            throw std::runtime_error("rejection leaked a payload");
    }
    else if (dots.qyooBits != dots.carrierTemplatePayload ||
             feature.dotDecStr != (populated ? "1" : "0"))
        throw std::runtime_error("legitimate zero/single-bit payload changed");
}


// Exercise the borrowed raw-image ingress used by native mobile adapters.
static void rawIngress(const char *path, uint64_t expected, bool mustDecode)
{
    FILE *file = std::fopen(path, "rb");
    if (!file) throw std::runtime_error("fixture missing");
    gdImagePtr source = gdImageCreateFromPng(file);
    std::fclose(file);
    if (!source) throw std::runtime_error("fixture decode failed");
    qyoo_detector_t *detector = nullptr;
    if (qyoo_detector_create(&detector) != QYOO_STATUS_OK)
        throw std::runtime_error("detector create failed");
    for (auto format : {QYOO_PIXEL_FORMAT_GRAY8, QYOO_PIXEL_FORMAT_BGRA32}) {
        qyoo_image_t image = {};
        image.struct_size = sizeof(image);
        image.width = gdImageSX(source);
        image.height = gdImageSY(source);
        image.pixel_format = format;
        int channels = format == QYOO_PIXEL_FORMAT_GRAY8 ? 1 : 4;
        image.stride_bytes = image.width * channels + 13;
        std::vector<uint8_t> bytes(image.stride_bytes * image.height, 0xa5);
        for (uint32_t y = 0; y < image.height; ++y)
            for (uint32_t x = 0; x < image.width; ++x) {
                int pixel = gdImageGetTrueColorPixel(source, x, y);
                uint8_t *out = &bytes[y * image.stride_bytes + x * channels];
                // All retained fixtures are grayscale, so this is exact.
                if (gdTrueColorGetRed(pixel) != gdTrueColorGetGreen(pixel) ||
                    gdTrueColorGetRed(pixel) != gdTrueColorGetBlue(pixel))
                    throw std::runtime_error("fixture is not grayscale");
                out[0] = gdTrueColorGetRed(pixel);
                if (channels == 4) out[1] = out[2] = out[0];
            }
        image.pixels = bytes.data();
        image.buffer_size = bytes.size();
        const auto original = bytes;
        qyoo_result_array_t results = {};
        auto status = qyoo_detector_detect(detector, &image, &results);
        if (status != QYOO_STATUS_OK) throw std::runtime_error("processing error");
        if (mustDecode && results.count != 1)
            throw std::runtime_error("known positive rejected");
        for (size_t i = 0; i < results.count; ++i)
            if (results.results[i].payload != expected)
                throw std::runtime_error("accepted incorrect raw-ingress payload");
        if (bytes != original) throw std::runtime_error("borrowed pixels modified");
        qyoo_result_array_release(&results);
        qyoo_result_array_release(&results);
    }
    qyoo_detector_destroy(detector);
    gdImageDestroy(source);
}

int main()
{
    try {
        for (bool white : {false, true}) {
            check(white, 0, false);
            for (int partialCells = 1; partialCells <= 6; ++partialCells)
                check(white, partialCells, false);
            check(white, 0, true);
            check(white, 0, false, 1); // Evidence omitted by the final 88px patch.
        }
        rawIngress("tests/fixtures/release-safety/c-max64.png", 68719476735ULL, false);
        rawIngress("tests/fixtures/release-safety/c-one128.png", 1, false);
        rawIngress("tests/fixtures/release-safety/c-one96.png", 1, false);
        rawIngress("tests/fixtures/public-smoke/single_bit_17.png", 262144, true);
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS: empty-grid evidence, both polarities and sparse payload\n";
}
