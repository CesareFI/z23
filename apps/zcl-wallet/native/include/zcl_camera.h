/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_CAMERA_H
#define ZCL_CAMERA_H
#include "zcl_qr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZCL_CAMERA_SIDE_MAX ((size_t)384)
#define ZCL_CAMERA_PACKET_MAX ((size_t)147461)

/* Camera IPC packet v1: version byte, width/height as LE16, then tightly packed
 * luminance. This is an app adapter format, never a Zclassic wire format.
 * Input uses zcl_scan_image_bounds. Integer point sampling preserves aspect
 * ratio approximately and bounds either output dimension at 384. No heap/I/O,
 * retained pointers, or secret imports. Caller owns non-overlapping spans for
 * the call; outputs remain unchanged on failure. Camera source memory is only
 * borrowed and must remain valid until this call returns. */
zcl_status zcl_camera_frame_pack(const uint8_t *image, size_t image_len,
                                  const zcl_qr_image *layout, uint8_t *packet,
                                  size_t capacity, size_t *packet_len);
/* Exact allocation size with the same source/sampling bounds as pack. Reads
 * no image bytes, allocates nothing and leaves packet_len unchanged on failure.
 * Packing independently rechecks bounds/capacity before writing any pixels. */
zcl_status zcl_camera_frame_size(size_t image_len, const zcl_qr_image *layout,
                                  size_t *packet_len);
/* Validates canonical packet length/dimensions, then decodes in C. */
zcl_status zcl_camera_packet_scan(const uint8_t *packet, size_t packet_len,
                                   zcl_network network, zcl_scanned_request *result);

#ifdef __cplusplus
}
#endif
#endif
