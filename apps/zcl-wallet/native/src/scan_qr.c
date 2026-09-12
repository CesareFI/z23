/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_qr.h"
#include "zcl_keys.h"
#include "quirc.h"
#include <stdlib.h>

_Static_assert(ZCL_SCAN_DIMENSION_MAX == QUIRC_MAX_IMAGE_DIMENSION,
               "Scanner and provider dimension limits must agree");

/* Fixed-size allocation, owned exclusively by decode_request until free. */
struct scan_workspace {
    struct quirc_code code;
    struct quirc_data data;
};

static bool valid_dimensions(const zcl_qr_image *layout)
{
    return layout->width >= 21 && layout->width <= ZCL_SCAN_DIMENSION_MAX &&
           layout->height >= 21 && layout->height <= ZCL_SCAN_DIMENSION_MAX;
}

zcl_status zcl_scan_image_bounds(size_t image_len, const zcl_qr_image *layout)
{
    if (layout == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (!valid_dimensions(layout))
        return ZCL_OUT_OF_RANGE;
    if (layout->pixel_stride < 1 || layout->pixel_stride > 4 || layout->row_stride > 8192)
        return ZCL_OUT_OF_RANGE;
    /* Operands were bounded above: row_bytes <=4093, needed <=8 MiB.
     * Validate before any allocation, pointer arithmetic or input read. */
    const size_t row_bytes = (layout->width - 1) * layout->pixel_stride + 1;
    if (layout->row_stride < row_bytes)
        return ZCL_OUT_OF_RANGE;
    const size_t needed = (layout->height - 1) * layout->row_stride + row_bytes;
    if (image_len < needed || image_len > ZCL_SCAN_INPUT_MAX)
        return ZCL_OUT_OF_RANGE;
    return ZCL_OK;
}

static zcl_status decode_payload(struct scan_workspace *work, zcl_network network,
                                  zcl_payment_request *request)
{
    quirc_decode_error_t decoded = quirc_decode(&work->code, &work->data);
    if (decoded == QUIRC_ERROR_DATA_ECC) {
        quirc_flip(&work->code);
        decoded = quirc_decode(&work->code, &work->data);
    }
    if (decoded != QUIRC_SUCCESS)
        return ZCL_INVALID_ENCODING;
    if (work->data.payload_len < 1 || (size_t)work->data.payload_len > ZCL_PAYMENT_TEXT_MAX)
        return ZCL_OUT_OF_RANGE;
    if (work->data.data_type == QUIRC_DATA_TYPE_KANJI ||
        (work->data.eci != 0 && work->data.eci != 26))
        return ZCL_UNSUPPORTED;
    return zcl_payment_parse(work->data.payload, (size_t)work->data.payload_len, network, request);
}

static zcl_status decode_request(const struct quirc *decoder, zcl_network network,
                                  zcl_payment_request *request)
{
    if (quirc_is_limited(decoder))
        return ZCL_RESOURCE_EXHAUSTED;
    const int count = quirc_count(decoder);
    if (count == 0)
        return ZCL_NOT_FOUND;
    if (count != 1)
        return ZCL_UNSUPPORTED;
    /* sizeof is representable by definition; constant count 1 cannot overflow.
     * One owner and one cleanup path, including decoding/parsing failures. */
    struct scan_workspace *work = calloc(1, sizeof(*work));
    if (work == NULL)
        return ZCL_RESOURCE_EXHAUSTED;
    quirc_extract(decoder, 0, &work->code);
    const zcl_status status = decode_payload(work, network, request);
    zcl_secure_zero(work, sizeof(*work));
    free(work);
    return status;
}

static zcl_status copy_image(struct quirc *decoder, const uint8_t *image,
                              const zcl_qr_image *layout)
{
    int width = 0, height = 0;
    uint8_t *destination = quirc_begin(decoder, &width, &height);
    if (destination == NULL || width != (int)layout->width || height != (int)layout->height)
        return ZCL_RESOURCE_EXHAUSTED;
    for (size_t y = 0; y < layout->height; ++y) {
        for (size_t x = 0; x < layout->width; ++x)
            destination[y * layout->width + x] = image[y * layout->row_stride + x * layout->pixel_stride];
    }
    return ZCL_OK;
}

static zcl_status scan_owned(struct quirc *decoder, const uint8_t *image,
                              const zcl_qr_image *layout, zcl_network network,
                              zcl_payment_request *request)
{
    /* Bounds already prove these casts and the provider allocation sizes. */
    if (quirc_resize(decoder, (int)layout->width, (int)layout->height) != 0)
        return ZCL_RESOURCE_EXHAUSTED;
    const zcl_status copied = copy_image(decoder, image, layout);
    if (copied != ZCL_OK)
        return copied;
    quirc_end(decoder);
    return decode_request(decoder, network, request);
}

zcl_status zcl_scan_qr(const uint8_t *image, size_t image_len,
                       const zcl_qr_image *layout, zcl_network network,
                       zcl_payment_request *request)
{
    if (image == NULL || request == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (network != ZCL_MAINNET && network != ZCL_TESTNET)
        return ZCL_UNSUPPORTED;
    const zcl_status bounds = zcl_scan_image_bounds(image_len, layout);
    if (bounds != ZCL_OK)
        return bounds;
    /* This call is the sole owner; quirc_destroy frees all provider buffers.
     * The provider never sees zero/excessive dimensions or caller pointers. */
    struct quirc *decoder = quirc_new();
    if (decoder == NULL)
        return ZCL_RESOURCE_EXHAUSTED;
    const zcl_status status = scan_owned(decoder, image, layout, network, request);
    quirc_destroy(decoder);
    return status;
}
