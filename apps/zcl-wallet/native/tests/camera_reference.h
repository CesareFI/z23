/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TEST_CAMERA_REFERENCE_H
#define ZCL_TEST_CAMERA_REFERENCE_H
#include "zcl_camera.h"

/* Independent test-only enumeration of the v1 point-sampling contract. This
 * checks a packet surrounded by 0xa5 in a ZCL_CAMERA_PACKET_MAX+2-byte span;
 * the caller's original output length is 17. No production helper is called. */
bool camera_reference_matches(const uint8_t *image, size_t image_len,
    const zcl_qr_image *layout, size_t capacity, zcl_status status,
    const uint8_t *guarded, size_t guarded_len, size_t written);
/* Independently enumerates the size query, including unchanged failed output. */
bool camera_reference_size_matches(size_t image_len, const zcl_qr_image *layout,
    zcl_status status, size_t written);
#endif
