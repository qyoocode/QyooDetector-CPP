#include "qyoo_detector.h"

_Static_assert(QYOO_PAYLOAD_BITS == 36u, "public payload width changed");
_Static_assert(QYOO_CARRIER_CORNER_COUNT == 4u, "public carrier geometry changed");

void compile_public_api(qyoo_detector_t *detector, const qyoo_image_t *image)
{
    qyoo_result_array_t results = {0};
    (void)qyoo_detector_detect(detector, image, &results);
    qyoo_result_array_release(&results);
}
