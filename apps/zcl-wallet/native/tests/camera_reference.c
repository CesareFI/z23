/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "camera_reference.h"

_Static_assert(ZCL_CAMERA_SIDE_MAX == 384 && ZCL_CAMERA_PACKET_MAX == 147461,
    "Reference describes the existing bounded camera packet v1");

struct sample_plan { size_t step, columns, rows, length; };

static bool source_bounds(size_t length, const zcl_qr_image *layout)
{
    if (layout->width < 21 || layout->width > 1024 || layout->height < 21 || layout->height > 1024)
        return false;
    if (layout->pixel_stride < 1 || layout->pixel_stride > 4 || layout->row_stride > 8192)
        return false;
    /* Enumerate the occupied row and row starts, independently of the core's
     * closed-form last-pixel expression. Proven maxima: 4093 and <8 MiB. */
    size_t needed = 1;
    for (size_t x = 1; x < layout->width; ++x) needed += layout->pixel_stride;
    if (needed > layout->row_stride) return false;
    for (size_t y = 1; y < layout->height; ++y) needed += layout->row_stride;
    return length >= needed && length <= ZCL_SCAN_INPUT_MAX;
}

static size_t sample_count(size_t side, size_t step)
{
    size_t count = 0;
    for (size_t position = 0; position < side; position += step) ++count;
    return count;
}

static zcl_status plan_samples(size_t length, const zcl_qr_image *layout,
    size_t capacity, struct sample_plan *plan)
{
    if (!source_bounds(length, layout)) return ZCL_OUT_OF_RANGE;
    /* Width/height<=1024 guarantee a fitting step in this finite enumeration.
     * This does not reuse the production ceiling-division calculation. */
    for (size_t step = 1; step <= 3; ++step) {
        const size_t columns = sample_count(layout->width, step);
        const size_t rows = sample_count(layout->height, step);
        if (columns > 384 || rows > 384) continue;
        if (columns < 21 || rows < 21) return ZCL_OUT_OF_RANGE;
        *plan = (struct sample_plan){step, columns, rows, 5 + columns * rows};
        return capacity < plan->length ? ZCL_BUFFER_TOO_SMALL : ZCL_OK;
    }
    return ZCL_OUT_OF_RANGE;
}

static bool unchanged(const uint8_t *guarded, size_t start, size_t length)
{
    for (size_t i = start; i < length; ++i) if (guarded[i] != 0xa5) return false;
    return true;
}

static bool header_matches(const uint8_t *packet, const struct sample_plan *plan)
{
    return packet[0] == 1 &&
        (size_t)packet[1] + (size_t)packet[2] * 256 == plan->columns &&
        (size_t)packet[3] + (size_t)packet[4] * 256 == plan->rows;
}

static bool pixels_match(const uint8_t *image, const zcl_qr_image *layout,
    const uint8_t *packet, const struct sample_plan *plan)
{
    size_t next = 5, row_start = 0;
    for (size_t y = 0; y < layout->height; y += plan->step) {
        size_t source = row_start;
        for (size_t x = 0; x < layout->width; x += plan->step) {
            if (packet[next++] != image[source]) return false;
            source += layout->pixel_stride * plan->step;
        }
        row_start += layout->row_stride * plan->step;
    }
    return next == plan->length;
}

bool camera_reference_matches(const uint8_t *image, size_t image_len,
    const zcl_qr_image *layout, size_t capacity, zcl_status status,
    const uint8_t *guarded, size_t guarded_len, size_t written)
{
    if (image == NULL || layout == NULL || guarded == NULL || guarded_len != ZCL_CAMERA_PACKET_MAX + 2)
        return false;
    struct sample_plan plan = {0};
    if (status != plan_samples(image_len, layout, capacity, &plan) || guarded[0] != 0xa5)
        return false;
    if (status != ZCL_OK) return written == 17 && unchanged(guarded, 0, guarded_len);
    if (written != plan.length || !header_matches(guarded + 1, &plan)) return false;
    return pixels_match(image, layout, guarded + 1, &plan) && unchanged(guarded, written + 1, guarded_len);
}
