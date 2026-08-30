#ifndef QYOO_DETECTOR_H
#define QYOO_DETECTOR_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(QYOO_DETECTOR_SHARED)
#  if defined(QYOO_DETECTOR_BUILDING)
#    define QYOO_API __declspec(dllexport)
#  else
#    define QYOO_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__)
#  define QYOO_API __attribute__((visibility("default")))
#else
#  define QYOO_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define QYOO_DETECTOR_API_VERSION 1u
#define QYOO_PAYLOAD_BITS 36u
#define QYOO_CARRIER_CORNER_COUNT 4u

typedef struct qyoo_detector qyoo_detector_t;

typedef uint32_t qyoo_status_t;
enum qyoo_status
{
    QYOO_STATUS_OK = 0,
    QYOO_STATUS_INVALID_ARGUMENT = 1,
    QYOO_STATUS_OUT_OF_MEMORY = 2,
    QYOO_STATUS_PROCESSING_FAILURE = 3
};

typedef uint32_t qyoo_pixel_format_t;
enum qyoo_pixel_format
{
    QYOO_PIXEL_FORMAT_GRAY8 = 1,
    QYOO_PIXEL_FORMAT_RGB24 = 2,
    QYOO_PIXEL_FORMAT_BGR24 = 3,
    QYOO_PIXEL_FORMAT_RGBA32 = 4,
    QYOO_PIXEL_FORMAT_BGRA32 = 5
};

typedef uint32_t qyoo_detection_outcome_t;
enum qyoo_detection_outcome
{
    QYOO_DETECTION_NO_QYOO = 0,
    QYOO_DETECTION_DECODED = 1
};

typedef uint32_t qyoo_result_status_t;
enum qyoo_result_status
{
    QYOO_RESULT_DECODED = 1
};

typedef uint32_t qyoo_normalization_mode_t;
enum qyoo_normalization_mode
{
    QYOO_NORMALIZATION_CARRIER_TEMPLATE = 1
};

typedef struct qyoo_image
{
    /* Set to sizeof(qyoo_image_t). */
    size_t struct_size;
    uint32_t width;
    uint32_t height;
    size_t stride_bytes;
    qyoo_pixel_format_t pixel_format;
    const uint8_t *pixels;
    /* Total readable bytes beginning at pixels. */
    size_t buffer_size;
} qyoo_image_t;

typedef struct qyoo_point
{
    double x;
    double y;
} qyoo_point_t;

typedef struct qyoo_bounds
{
    double min_x;
    double min_y;
    double max_x;
    double max_y;
} qyoo_bounds_t;

typedef struct qyoo_result
{
    qyoo_result_status_t status;
    uint64_t payload;
    uint32_t payload_bit_count;
    qyoo_bounds_t bounds;
    /* Model corners (0,0), (1,0), (1,1), (0,1), in original-image pixels. */
    qyoo_point_t carrier_quadrilateral[QYOO_CARRIER_CORNER_COUNT];
    uint32_t carrier_quadrilateral_valid;
    qyoo_normalization_mode_t normalization_mode;
} qyoo_result_t;

typedef struct qyoo_result_array
{
    qyoo_detection_outcome_t outcome;
    size_t count;
    qyoo_result_t *results;
} qyoo_result_array_t;

QYOO_API uint32_t qyoo_detector_api_version(void);

/*
 * Creates an independent detector context. On success, *out_detector is owned
 * by the caller and must be passed to qyoo_detector_destroy().
 */
QYOO_API qyoo_status_t qyoo_detector_create(qyoo_detector_t **out_detector);
QYOO_API void qyoo_detector_destroy(qyoo_detector_t *detector);

/*
 * Detects zero or more physical Qyoos with the frozen production policy.
 * The image and its pixel buffer are borrowed only for this call and are never
 * retained or modified. Rows may have padding; stride_bytes is the byte distance
 * between row starts. Alpha bytes, where present, are ignored.
 *
 * On QYOO_STATUS_OK, out_results->outcome explicitly distinguishes a safe
 * no-Qyoo rejection from one or more decoded results. Processing failures are
 * returned as a non-OK qyoo_status_t. out_results must be zero-initialized or
 * previously released; it must not contain a live result allocation. The
 * caller owns each successful result allocation and must release it only with
 * qyoo_result_array_release().
 *
 * Calls using one context are serialized internally. Calls using different
 * contexts may execute concurrently. Destroying a context concurrently with a
 * call using that context is not allowed.
 */
QYOO_API qyoo_status_t qyoo_detector_detect(qyoo_detector_t *detector,
                                            const qyoo_image_t *image,
                                            qyoo_result_array_t *out_results);

/* Safe for an empty or already-released array; resets all fields. */
QYOO_API void qyoo_result_array_release(qyoo_result_array_t *results);

QYOO_API const char *qyoo_status_string(qyoo_status_t status);

#ifdef __cplusplus
}
#endif

#endif
