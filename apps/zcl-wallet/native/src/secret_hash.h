/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_SECRET_HASH_H
#define ZCL_SECRET_HASH_H
#include "zcl_keys.h"

/* Internal bounded HMAC. Inputs <=256/512 bytes; output capacity >=64.
 * Caller owns all spans. Scratch is cleared; output unchanged on failure. */
zcl_status zcl_hmac_sha512(const uint8_t *key, size_t key_len,
                          const uint8_t *data, size_t data_len,
                          uint8_t *output, size_t output_capacity);
#endif
