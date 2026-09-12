/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_camera.h"

static bool valid_output(size_t width, size_t height)
{
    return width >= 21 && width <= ZCL_CAMERA_SIDE_MAX &&
           height >= 21 && height <= ZCL_CAMERA_SIDE_MAX;
}

static void sample_pixels(const uint8_t *image, const zcl_qr_image *source,
                            uint8_t *pixels, const zcl_qr_image *target, size_t step)
{
    for (size_t y = 0; y < target->height; ++y) {
        for (size_t x = 0; x < target->width; ++x)
            pixels[y * target->width + x] =
                image[y * step * source->row_stride + x * step * source->pixel_stride];
    }
}

static zcl_status pack_validated(const uint8_t *image, const zcl_qr_image *layout,
                                  uint8_t *packet, size_t capacity, size_t *packet_len)
{
    const size_t largest = layout->width > layout->height ? layout->width : layout->height;
    /* Input dimensions <=1024 prove these sums/products before any write. */
    const size_t step = (largest + ZCL_CAMERA_SIDE_MAX - 1) / ZCL_CAMERA_SIDE_MAX;
    const size_t width = (layout->width + step - 1) / step;
    const size_t height = (layout->height + step - 1) / step;
    if (!valid_output(width, height))
        return ZCL_OUT_OF_RANGE;
    const size_t length = 5 + width * height;
    if (capacity < length)
        return ZCL_BUFFER_TOO_SMALL;
    const zcl_qr_image target = {width, height, width, 1};
    sample_pixels(image, layout, packet + 5, &target, step);
    packet[0] = 1;
    packet[1] = (uint8_t)(width & 255);
    packet[2] = (uint8_t)(width >> 8);
    packet[3] = (uint8_t)(height & 255);
    packet[4] = (uint8_t)(height >> 8);
    *packet_len = length;
    return ZCL_OK;
}

zcl_status zcl_camera_frame_pack(const uint8_t *image, size_t image_len,
                                  const zcl_qr_image *layout, uint8_t *packet,
                                  size_t capacity, size_t *packet_len)
{
    if (image == NULL || packet == NULL || packet_len == NULL)
        return ZCL_INVALID_ARGUMENT;
    const zcl_status bounds = zcl_scan_image_bounds(image_len, layout);
    if (bounds != ZCL_OK)
        return bounds;
    return pack_validated(image, layout, packet, capacity, packet_len);
}

static zcl_status packet_layout(const uint8_t *packet, size_t packet_len, zcl_qr_image *layout)
{
    if (packet == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (packet_len < 5 || packet_len > ZCL_CAMERA_PACKET_MAX)
        return ZCL_OUT_OF_RANGE;
    if (packet[0] != 1)
        return ZCL_UNSUPPORTED;
    const size_t width = (size_t)packet[1] + ((size_t)packet[2] << 8);
    const size_t height = (size_t)packet[3] + ((size_t)packet[4] << 8);
    if (!valid_output(width, height))
        return ZCL_OUT_OF_RANGE;
    if (packet_len != 5 + width * height)
        return ZCL_INVALID_ENCODING;
    *layout = (zcl_qr_image){width, height, width, 1};
    return ZCL_OK;
}

zcl_status zcl_camera_packet_scan(const uint8_t *packet, size_t packet_len,
                                   zcl_network network, zcl_scanned_request *result)
{
    if (result == NULL)
        return ZCL_INVALID_ARGUMENT;
    zcl_qr_image layout = {0};
    const zcl_status status = packet_layout(packet, packet_len, &layout);
    if (status != ZCL_OK)
        return status;
    return zcl_scan_request(packet + 5, packet_len - 5, &layout, network, result);
}
