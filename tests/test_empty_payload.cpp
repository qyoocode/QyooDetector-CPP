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
static void check(bool white, int partialCells, bool populated)
{
    gdImagePtr image = gdImageCreateTrueColor(88, 88);
    if (!image) throw std::runtime_error("image allocation failed");
    Feature feature;
    feature.imgSizeX = feature.imgSizeY = 88;
    FeatureDotsProcessor dots(image, nullptr, &feature,
        NormalizationCarrierTemplate, "", QualifiedAffineFallback, false, false);
    gdImageDestroy(image);
    dots.normalizationAvailable = true;
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
    bool shouldReject = partialCells == 6 && !populated;
    if (dots.carrierTemplateEmptyPayloadRejected != shouldReject)
        throw std::runtime_error("unexpected empty-payload decision");
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

static void checkRawRejection(const char *path)
{
    std::ifstream input(path, std::ios::binary);
    std::vector<uint8_t> pixels((std::istreambuf_iterator<char>(input)), {});
    if (pixels.size() != 720 * 1280 * 4)
        throw std::runtime_error("expected exact 720x1280 packed BGRA frame");
    qyoo_image_t image = {};
    image.struct_size = sizeof(image);
    image.width = 720; image.height = 1280; image.stride_bytes = 720 * 4;
    image.pixel_format = QYOO_PIXEL_FORMAT_BGRA32;
    image.pixels = pixels.data(); image.buffer_size = pixels.size();
    qyoo_detector_t *detector = nullptr;
    if (qyoo_detector_create(&detector) != QYOO_STATUS_OK)
        throw std::runtime_error("detector allocation failed");
    qyoo_result_array_t results = {};
    auto status = qyoo_detector_detect(detector, &image, &results);
    bool rejected = status == QYOO_STATUS_OK &&
        results.outcome == QYOO_DETECTION_NO_QYOO && results.count == 0 && !results.results;
    qyoo_result_array_release(&results);
    qyoo_detector_destroy(detector);
    if (!rejected) throw std::runtime_error("raw incident frame was not safely rejected");
    std::cout << "PASS: exact raw incident rejected: " << path << '\n';
}

int main(int argc, char **argv)
{
    try {
        for (bool white : {false, true}) {
            check(white, 0, false);  // Affirmatively empty: valid ID 0.
            check(white, 5, false);  // 25 residual pixels: below one dot's support.
            check(white, 6, false);  // 30 residual pixels: no cell is a dot.
            check(white, 0, true);   // Ordinary sparse payload remains exact.
        }
        // Opt-in private incident fixtures; originals stay outside published assets.
        for (int index = 1; index < argc; ++index) checkRawRejection(argv[index]);
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS: empty-payload evidence, both polarities, sparse payload\n";
}
