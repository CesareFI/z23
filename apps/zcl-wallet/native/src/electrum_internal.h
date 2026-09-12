/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_ELECTRUM_INTERNAL_H
#define ZCL_ELECTRUM_INTERNAL_H
#include "rpc_json.h"
zcl_status zcl_rpc_hex(const zcl_rpc_json *document, const zcl_rpc_token *token,
                        uint8_t *output, size_t capacity, size_t *length);
zcl_status zcl_rpc_header_hash(const zcl_rpc_json *document, const zcl_rpc_token *token,
                                zcl_network network, uint32_t height, uint8_t hash[32]);
#endif
