/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_SCAN_FIXTURE_H
#define ZCL_SCAN_FIXTURE_H
#include "zcl_qr.h"
/* Public fixtures only. Returned allocation belongs to caller, freed once. */
uint8_t *scan_fixture(const uint8_t *text, size_t text_len, size_t scale,
                       unsigned int rotation, size_t pixel_stride, size_t padding,
                       zcl_qr_image *layout, size_t *image_len);
#endif
