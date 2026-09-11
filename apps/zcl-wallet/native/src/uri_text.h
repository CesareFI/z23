/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_URI_TEXT_H
#define ZCL_URI_TEXT_H
#include "zcl_wallet.h"
zcl_status zcl_uri_decode_field(const uint8_t *raw, size_t raw_len,
                               uint8_t *output, size_t capacity, size_t *length);
zcl_status zcl_uri_validate_text(const uint8_t *text, size_t length);
zcl_status zcl_utf8_visible_text(const uint8_t *text, size_t length);
#endif
