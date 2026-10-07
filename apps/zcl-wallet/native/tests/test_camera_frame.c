/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_camera.h"
#include "scan_fixture.h"
#include "scan_result_reference.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Camera check failed at %d\n", __LINE__); abort(); } } while (0)
/* Public fixtures, static only in this single-threaded test executable. */
static uint8_t packet[ZCL_CAMERA_PACKET_MAX + 2];
static uint8_t source[480 * 1312];

static void samples(void)
{
    const zcl_qr_image layout = {640, 480, 1312, 2};
    source[0] = 7;
    source[4] = 11;
    source[2 * 1312] = 17;
    source[478 * 1312 + 638 * 2] = 251;
    memset(packet, 0xa5, sizeof(packet));
    size_t length = 0;
    CHECK(zcl_camera_frame_pack(source, sizeof(source), &layout, packet + 1,
                               ZCL_CAMERA_PACKET_MAX, &length) == ZCL_OK);
    const uint8_t header[] = {1, 64, 1, 240, 0};
    CHECK(length == 5 + 320 * 240);
    CHECK(memcmp(packet + 1, header, sizeof(header)) == 0);
    CHECK(packet[6] == 7 && packet[7] == 11 && packet[326] == 17);
    CHECK(packet[length] == 251);
    CHECK(packet[0] == 0xa5 && packet[length + 1] == 0xa5);
    CHECK(source[0] == 7 && source[4] == 11 && source[2 * 1312] == 17);
}

static void pack_refuses(size_t input_len, const zcl_qr_image *layout, size_t capacity)
{
    memset(packet, 0xa5, sizeof(packet));
    size_t length = 71;
    CHECK(zcl_camera_frame_pack(source, input_len, layout, packet, capacity, &length) != ZCL_OK);
    CHECK(length == 71);
    for (size_t i = 0; i < sizeof(packet); ++i) CHECK(packet[i] == 0xa5);
}

static void pack_bounds(void)
{
    const zcl_qr_image good = {640, 480, 1312, 2};
    pack_refuses(sizeof(source), &good, 5 + 320 * 240 - 1);
    pack_refuses(479 * 1312 + 639 * 2, &good, sizeof(packet));
    pack_refuses(SIZE_MAX, &good, sizeof(packet));
    pack_refuses(sizeof(source), NULL, sizeof(packet));
    zcl_qr_image bad = {SIZE_MAX, 480, 1312, 2};
    pack_refuses(sizeof(source), &bad, sizeof(packet));
    bad = (zcl_qr_image){640, 480, SIZE_MAX, 2};
    pack_refuses(sizeof(source), &bad, sizeof(packet));
    bad = (zcl_qr_image){21, 1024, 21, 1};
    pack_refuses(21 * 1024, &bad, sizeof(packet));
    size_t length = 0;
    CHECK(zcl_camera_frame_pack(NULL, sizeof(source), &good, packet, sizeof(packet), &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_camera_frame_pack(source, sizeof(source), &good, NULL, sizeof(packet), &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_camera_frame_pack(source, sizeof(source), &good, packet, sizeof(packet), NULL) == ZCL_INVALID_ARGUMENT);
}

static void scan_refuses(const uint8_t *input, size_t length)
{
    zcl_scanned_request result, before;
    memset(&result, 0xa5, sizeof(result));
    memcpy(&before, &result, sizeof(before));
    CHECK(zcl_camera_packet_scan(input, length, ZCL_MAINNET, &result) != ZCL_OK);
    CHECK(memcmp(&result, &before, sizeof(result)) == 0);
}

static void packets(void)
{
    memset(packet, 255, sizeof(packet));
    const uint8_t header[] = {1, 21, 0, 21, 0};
    memcpy(packet, header, sizeof(header));
    scan_refuses(NULL, 446);
    scan_refuses(packet, 4);
    scan_refuses(packet, SIZE_MAX);
    scan_refuses(packet, 445);
    scan_refuses(packet, 447);
    scan_refuses(packet, 446); /* White image is not a QR code. */
    packet[0] = 2;
    scan_refuses(packet, 446);
    packet[0] = 1;
    packet[2] = 255;
    scan_refuses(packet, 446);
    CHECK(zcl_camera_packet_scan(packet, 446, ZCL_MAINNET, NULL) == ZCL_INVALID_ARGUMENT);
}

static const uint8_t request[] = "zclassic:t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF?amount=1.25&label=Public%20fixture&message=Hello";

static void mismatch_then_restore(const zcl_payment_request *expected, zcl_payment_request *changed)
{
    CHECK(!scan_result_matches(request, sizeof(request) - 1, ZCL_MAINNET, changed));
    *changed = *expected;
}

static void corrupt_address(const zcl_payment_request *expected)
{
    zcl_payment_request changed = *expected;
    changed.address.network = ZCL_TESTNET;
    mismatch_then_restore(expected, &changed);
    changed.address.kind = ZCL_P2SH;
    mismatch_then_restore(expected, &changed);
    for (size_t i = 0; i < sizeof(changed.address.hash); ++i) {
        changed.address.hash[i] ^= UINT8_C(1);
        mismatch_then_restore(expected, &changed);
    }
    for (size_t i = 0; i < sizeof(changed.address_text); ++i) {
        changed.address_text[i] ^= UINT8_C(1);
        mismatch_then_restore(expected, &changed);
    }
}

static void corrupt_fields(const zcl_payment_request *expected)
{
    zcl_payment_request changed = *expected;
    changed.has_amount = false;
    mismatch_then_restore(expected, &changed);
    changed.has_label = false;
    mismatch_then_restore(expected, &changed);
    changed.has_message = false;
    mismatch_then_restore(expected, &changed);
    ++changed.amount;
    mismatch_then_restore(expected, &changed);
    changed.label_len = SIZE_MAX;
    mismatch_then_restore(expected, &changed);
    changed.message_len = SIZE_MAX;
    mismatch_then_restore(expected, &changed);
    for (size_t i = 0; i < expected->label_len; ++i) {
        changed.label[i] ^= UINT8_C(1);
        mismatch_then_restore(expected, &changed);
    }
    for (size_t i = 0; i < expected->message_len; ++i) {
        changed.message[i] ^= UINT8_C(1);
        mismatch_then_restore(expected, &changed);
    }
}

static void request_text(void)
{
    zcl_qr_image layout = {0};
    size_t image_len = 0, length = 0;
    uint8_t *image = scan_fixture(request, sizeof(request) - 1, 4, 0, 2, 7, &layout, &image_len);
    CHECK(image != NULL);
    CHECK(zcl_camera_frame_pack(image, image_len, &layout, packet, sizeof(packet), &length) == ZCL_OK);
    zcl_scanned_request decoded = {0};
    CHECK(zcl_camera_packet_scan(packet, length, ZCL_MAINNET, &decoded) == ZCL_OK);
    CHECK(decoded.text_len == sizeof(request) - 1);
    CHECK(memcmp(decoded.text, request, decoded.text_len) == 0);
    CHECK(decoded.request.has_amount && decoded.request.amount == UINT64_C(125000000));
    CHECK(scan_result_matches(request, sizeof(request) - 1, ZCL_MAINNET, &decoded.request));
    CHECK(!scan_result_matches(NULL, sizeof(request) - 1, ZCL_MAINNET, &decoded.request));
    CHECK(!scan_result_matches(request, 0, ZCL_MAINNET, &decoded.request));
    CHECK(!scan_result_matches(request, sizeof(request) - 1, ZCL_TESTNET, &decoded.request));
    CHECK(!scan_result_matches(request, sizeof(request) - 1, ZCL_MAINNET, NULL));
    corrupt_address(&decoded.request);
    corrupt_fields(&decoded.request);
    CHECK(zcl_camera_packet_scan(packet, length, ZCL_TESTNET, &decoded) != ZCL_OK);
    free(image);
}

int main(void)
{
    samples();
    pack_bounds();
    packets();
    request_text();
    puts("Camera sampling, canaries, canonical packets and exact public request text passed");
    return 0;
}
