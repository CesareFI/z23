/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_EC_CONTEXT_H
#define ZCL_EC_CONTEXT_H

#include "zcl_keys.h"
#include <secp256k1.h>

/* Caller owns this invocation-local object and calls end exactly once after
 * begin, including failures. No copying while active. storage is the sole
 * owning allocation pointer; handle borrows that allocation. The checked
 * allocation supplies alignment and storage without a declared effective type.
 * Only the provider accesses storage until its context has been destroyed. */
typedef struct {
    void *storage;
    size_t storage_len;
    secp256k1_context *handle;
} zcl_ec_context;

zcl_status zcl_ec_begin(zcl_ec_context *context, const uint8_t *blinding, size_t blinding_len);
void zcl_ec_end(zcl_ec_context *context);
zcl_status zcl_ec_public(const secp256k1_context *context,
                        const uint8_t *secret, size_t secret_len,
                        uint8_t *public_key, size_t public_key_capacity);
#endif
