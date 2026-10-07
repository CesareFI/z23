/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_camera.h"
#include "camera_reference.h"
#include "scan_result_reference.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void scan_packet(const uint8_t *data, size_t size, zcl_network network)
{
    zcl_scanned_request result, before;
    memset(&result, 0xa5, sizeof(result));
    memcpy(&before, &result, sizeof(before));
    const zcl_status status = zcl_camera_packet_scan(data, size, network, &result);
    if (status != ZCL_OK && memcmp(&before, &result, sizeof(result)) != 0) abort();
    if (status == ZCL_OK) {
        if (result.text_len == 0 || result.text_len > ZCL_PAYMENT_TEXT_MAX) abort();
        if (!scan_result_matches(result.text, result.text_len, network, &result.request)) {
            fputs("Camera request differs from decoded text\n", stderr);
            abort();
        }
    }
}

static void check_sizing(size_t length, const zcl_qr_image *layout, size_t capacity,
    zcl_status packing, size_t written)
{
    size_t measured = 17;
    const zcl_status sizing = zcl_camera_frame_size(length, layout, &measured);
    if (packing == ZCL_OK) {
        if (sizing != ZCL_OK || measured != written) abort();
    } else if (packing == ZCL_BUFFER_TOO_SMALL) {
        if (sizing != ZCL_OK || measured <= capacity || measured > ZCL_CAMERA_PACKET_MAX) abort();
    } else if (sizing != packing || measured != 17) {
        abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 8 || size > ZCL_SCAN_INPUT_MAX + 8) return 0;
    const zcl_network network = (data[7] & 1) ? ZCL_MAINNET : ZCL_TESTNET;
    scan_packet(data, size, network);
    const zcl_qr_image layout = {(size_t)data[0] + (size_t)data[1] * 256,
        (size_t)data[2] + (size_t)data[3] * 256,
        (size_t)data[4] + (size_t)data[5] * 256, (size_t)data[6]};
    /* Constant checked allocation, owned/freed solely by this fuzz iteration. */
    uint8_t *packet = malloc(ZCL_CAMERA_PACKET_MAX + 2);
    if (packet == NULL) return 0;
    memset(packet, 0xa5, ZCL_CAMERA_PACKET_MAX + 2);
    size_t written = 17;
    const size_t capacity = (data[7] & 2) ? (size_t)data[0] : ZCL_CAMERA_PACKET_MAX;
    const zcl_status status = zcl_camera_frame_pack(data + 8, size - 8, &layout, packet + 1, capacity, &written);
    if (!camera_reference_matches(data + 8, size - 8, &layout, capacity, status,
        packet, ZCL_CAMERA_PACKET_MAX + 2, written)) abort();
    check_sizing(size - 8, &layout, capacity, status, written);
    /* The reference subsumes the former length/guard/failure checks and also
     * checks exact status, header, every pixel and the complete untouched tail. */
    if (status == ZCL_OK) scan_packet(packet + 1, written, network);
    free(packet);
    return 0;
}
