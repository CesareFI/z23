/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_QR_H
#define ZCL_QR_H
#include "zcl_wallet.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZCL_RECEIVE_QR_SIDE_MAX ((size_t)41)
#define ZCL_RECEIVE_QR_MODULES_MAX ((size_t)1681)

/* Validated public transparent addresses only. Returns a row-major square of
 * bytes (0=white, 1=black), including a four-module white border on each side.
 * On success exactly side*side bytes are written. On failure all outputs are
 * unchanged. Buffers are caller-owned for the call and must not overlap each
 * other or side. No heap allocation, I/O, retained pointers or secret input.
 */
zcl_status zcl_receive_qr(const uint8_t *text, size_t text_len, zcl_network network,
                          uint8_t *modules, size_t capacity, size_t *side);

#ifdef __cplusplus
}
#endif
#endif
