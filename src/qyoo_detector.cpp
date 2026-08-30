#include "qyoo_detector.h"

#include <climits>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>

#include <gd.h>

#include "DetectorCore.h"

struct qyoo_detector
{
    std::mutex callMutex;
};

namespace
{
class GdImageOwner
{
public:
    explicit GdImageOwner(gdImagePtr image) : image_(image) { }
    ~GdImageOwner()
    {
        if (image_)
            gdImageDestroy(image_);
    }
    gdImagePtr get() const { return image_; }

private:
    GdImageOwner(const GdImageOwner &);
    GdImageOwner &operator=(const GdImageOwner &);
    gdImagePtr image_;
};

size_t bytesPerPixel(qyoo_pixel_format_t format)
{
    switch (format)
    {
        case QYOO_PIXEL_FORMAT_GRAY8: return 1;
        case QYOO_PIXEL_FORMAT_RGB24:
        case QYOO_PIXEL_FORMAT_BGR24: return 3;
        case QYOO_PIXEL_FORMAT_RGBA32:
        case QYOO_PIXEL_FORMAT_BGRA32: return 4;
    }
    return 0;
}

qyoo_status_t validateImage(const qyoo_image_t *image)
{
    if (!image || image->struct_size < sizeof(qyoo_image_t) || !image->pixels ||
        image->width == 0 || image->height == 0 ||
        image->width > static_cast<uint32_t>(INT_MAX) ||
        image->height > static_cast<uint32_t>(INT_MAX) ||
        image->width > static_cast<uint32_t>(INT_MAX) / image->height)
        return QYOO_STATUS_INVALID_ARGUMENT;
    const size_t pixelBytes = bytesPerPixel(image->pixel_format);
    if (pixelBytes == 0 || image->width > std::numeric_limits<size_t>::max() / pixelBytes)
        return QYOO_STATUS_INVALID_ARGUMENT;
    const size_t rowBytes = static_cast<size_t>(image->width) * pixelBytes;
    if (image->stride_bytes < rowBytes)
        return QYOO_STATUS_INVALID_ARGUMENT;
    const size_t precedingRows = static_cast<size_t>(image->height - 1);
    if (precedingRows > 0 &&
        image->stride_bytes > (std::numeric_limits<size_t>::max() - rowBytes) / precedingRows)
        return QYOO_STATUS_INVALID_ARGUMENT;
    const size_t requiredBytes = precedingRows * image->stride_bytes + rowBytes;
    if (image->buffer_size < requiredBytes)
        return QYOO_STATUS_INVALID_ARGUMENT;
    return QYOO_STATUS_OK;
}

gdImagePtr decodedImage(const qyoo_image_t &input)
{
    gdImagePtr image = gdImageCreateTrueColor(static_cast<int>(input.width),
                                              static_cast<int>(input.height));
    if (!image)
        return nullptr;
    const size_t pixelBytes = bytesPerPixel(input.pixel_format);
    for (uint32_t y = 0; y < input.height; ++y)
    {
        const uint8_t *row = input.pixels + static_cast<size_t>(y) * input.stride_bytes;
        for (uint32_t x = 0; x < input.width; ++x)
        {
            const uint8_t *pixel = row + static_cast<size_t>(x) * pixelBytes;
            int red = 0;
            int green = 0;
            int blue = 0;
            switch (input.pixel_format)
            {
                case QYOO_PIXEL_FORMAT_GRAY8:
                    red = green = blue = pixel[0];
                    break;
                case QYOO_PIXEL_FORMAT_RGB24:
                case QYOO_PIXEL_FORMAT_RGBA32:
                    red = pixel[0]; green = pixel[1]; blue = pixel[2];
                    break;
                case QYOO_PIXEL_FORMAT_BGR24:
                case QYOO_PIXEL_FORMAT_BGRA32:
                    blue = pixel[0]; green = pixel[1]; red = pixel[2];
                    break;
            }
            gdImageSetPixel(image, static_cast<int>(x), static_cast<int>(y),
                            gdTrueColor(red, green, blue));
        }
    }
    return image;
}

void initializeResults(qyoo_result_array_t *results)
{
    results->outcome = QYOO_DETECTION_NO_QYOO;
    results->count = 0;
    results->results = nullptr;
}
}

extern "C" uint32_t qyoo_detector_api_version(void)
{
    return QYOO_DETECTOR_API_VERSION;
}

extern "C" qyoo_status_t qyoo_detector_create(qyoo_detector_t **out_detector)
{
    if (!out_detector)
        return QYOO_STATUS_INVALID_ARGUMENT;
    *out_detector = nullptr;
    try
    {
        *out_detector = new qyoo_detector_t();
        return QYOO_STATUS_OK;
    }
    catch (const std::bad_alloc &)
    {
        return QYOO_STATUS_OUT_OF_MEMORY;
    }
    catch (...)
    {
        return QYOO_STATUS_PROCESSING_FAILURE;
    }
}

extern "C" void qyoo_detector_destroy(qyoo_detector_t *detector)
{
    delete detector;
}

extern "C" qyoo_status_t qyoo_detector_detect(qyoo_detector_t *detector,
                                                const qyoo_image_t *image,
                                                qyoo_result_array_t *out_results)
{
    if (!out_results)
        return QYOO_STATUS_INVALID_ARGUMENT;
    initializeResults(out_results);
    if (!detector)
        return QYOO_STATUS_INVALID_ARGUMENT;
    qyoo_status_t validation = validateImage(image);
    if (validation != QYOO_STATUS_OK)
        return validation;

    std::lock_guard<std::mutex> lock(detector->callMutex);
    try
    {
        GdImageOwner decoded(decodedImage(*image));
        if (!decoded.get())
            return QYOO_STATUS_OUT_OF_MEMORY;
        DetectorCoreOptions options;
        DetectorCoreResult core = runDetectorCore(decoded.get(), options);
        if (core.status != DetectorCoreSuccess)
            return QYOO_STATUS_PROCESSING_FAILURE;
        if (core.candidates.empty())
            return QYOO_STATUS_OK;
        if (core.candidates.size() >
            std::numeric_limits<size_t>::max() / sizeof(qyoo_result_t))
            return QYOO_STATUS_OUT_OF_MEMORY;
        std::vector<uint64_t> payloads;
        payloads.reserve(core.candidates.size());
        for (const LocalizationCandidate &candidate : core.candidates)
            payloads.push_back(std::stoull(candidate.payload, nullptr, 2));
        qyoo_result_t *results = static_cast<qyoo_result_t *>(
            std::calloc(core.candidates.size(), sizeof(qyoo_result_t)));
        if (!results)
            return QYOO_STATUS_OUT_OF_MEMORY;
        for (size_t index = 0; index < core.candidates.size(); ++index)
        {
            const LocalizationCandidate &candidate = core.candidates[index];
            qyoo_result_t &result = results[index];
            result.status = QYOO_RESULT_DECODED;
            result.payload = payloads[index];
            result.payload_bit_count = QYOO_PAYLOAD_BITS;
            result.bounds.min_x = candidate.bounds.minX;
            result.bounds.min_y = candidate.bounds.minY;
            result.bounds.max_x = candidate.bounds.maxX;
            result.bounds.max_y = candidate.bounds.maxY;
            for (size_t corner = 0; corner < QYOO_CARRIER_CORNER_COUNT; ++corner)
            {
                result.carrier_quadrilateral[corner].x =
                    candidate.carrierQuadrilateral[corner].x;
                result.carrier_quadrilateral[corner].y =
                    candidate.carrierQuadrilateral[corner].y;
            }
            result.carrier_quadrilateral_valid = candidate.carrierQuadrilateralValid ? 1 : 0;
            result.normalization_mode = QYOO_NORMALIZATION_CARRIER_TEMPLATE;
        }
        out_results->outcome = QYOO_DETECTION_DECODED;
        out_results->count = core.candidates.size();
        out_results->results = results;
        return QYOO_STATUS_OK;
    }
    catch (const std::bad_alloc &)
    {
        return QYOO_STATUS_OUT_OF_MEMORY;
    }
    catch (...)
    {
        return QYOO_STATUS_PROCESSING_FAILURE;
    }
}

extern "C" void qyoo_result_array_release(qyoo_result_array_t *results)
{
    if (!results)
        return;
    std::free(results->results);
    initializeResults(results);
}

extern "C" const char *qyoo_status_string(qyoo_status_t status)
{
    switch (status)
    {
        case QYOO_STATUS_OK: return "ok";
        case QYOO_STATUS_INVALID_ARGUMENT: return "invalid argument";
        case QYOO_STATUS_OUT_OF_MEMORY: return "out of memory";
        case QYOO_STATUS_PROCESSING_FAILURE: return "processing failure";
    }
    return "unknown status";
}
