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

#define ZCL_SCAN_DIMENSION_MAX ((size_t)1024)
#define ZCL_SCAN_INPUT_MAX ((size_t)8388608)
typedef struct {
    size_t width;
    size_t height;
    size_t row_stride;
    size_t pixel_stride;
} zcl_qr_image;

/* The exact validated public text accompanies its parsed view for transfer to
 * an isolated decoder's caller. That caller must independently parse it again.
 * text_len bytes are meaningful; remaining text bytes are not serialized. */
typedef struct {
    zcl_payment_request request;
    uint8_t text[ZCL_PAYMENT_TEXT_MAX];
    size_t text_len;
} zcl_scanned_request;

/* Public camera luminance only. One visible QR, decoded and validated as a
 * receiving address/payment request for the explicit network. Never imports
 * secrets or authorizes payment. Dimensions 21..1024, pixel stride 1..4, row
 * stride <=8192 and input <=8 MiB. The final row need not include padding.
 * Caller owns stable non-overlapping input/layout/output for this call.
 * Output is unchanged on failure. This function owns and frees a bounded
 * decoder and workspace on every path; no retained pointers or global state.
 */
zcl_status zcl_scan_qr(const uint8_t *image, size_t image_len,
                       const zcl_qr_image *layout, zcl_network network,
                       zcl_payment_request *request);

/* Same ownership/bounds as zcl_scan_qr; output is unchanged on failure. */
zcl_status zcl_scan_request(const uint8_t *image, size_t image_len,
                            const zcl_qr_image *layout, zcl_network network,
                            zcl_scanned_request *result);

/* Validate spans without reading the image or allocating. Useful to the JNI
 * adapter before copying a camera plane. No outputs or retained pointers. */
zcl_status zcl_scan_image_bounds(size_t image_len, const zcl_qr_image *layout);

#ifdef __cplusplus
}
#endif
#endif
