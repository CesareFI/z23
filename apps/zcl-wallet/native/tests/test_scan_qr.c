/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "scan_fixture.h"
#include "scan_result_reference.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Scan check failed at %d\n", __LINE__); abort(); } } while (0)
static const uint8_t address[] = "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF";

static void accepted(void)
{
    uint8_t hash[20] = {0}, text[35];
    for (unsigned int fixture = 0; fixture < 16; ++fixture) {
        const zcl_network network = (fixture % 2 == 0) ? ZCL_MAINNET : ZCL_TESTNET;
        memset(hash, (int)(fixture * 17), sizeof(hash));
        size_t text_len = 0;
        CHECK(zcl_address_from_hash(hash, sizeof(hash), network, text, sizeof(text), &text_len) == ZCL_OK);
        for (unsigned int rotation = 0; rotation < 4; ++rotation) {
            zcl_qr_image layout = {0};
            size_t image_len = 0;
            uint8_t *image = scan_fixture(text, text_len, 3, rotation, rotation + 1, 7, &layout, &image_len);
            CHECK(image != NULL);
            zcl_payment_request result = {0};
            CHECK(zcl_scan_qr(image, image_len, &layout, network, &result) == ZCL_OK);
            CHECK(scan_result_matches(text, text_len, network, &result));
            CHECK(memcmp(result.address_text, text, text_len) == 0);
            CHECK(result.address.network == network && !result.has_amount);
            /* Last-row padding is optional. */
            size_t needed = (layout.height - 1) * layout.row_stride +
                            (layout.width - 1) * layout.pixel_stride + 1;
            CHECK(zcl_scan_qr(image, needed, &layout, network, &result) == ZCL_OK);
            CHECK(scan_result_matches(text, text_len, network, &result));
            free(image);
        }
    }
}

static void payment(void)
{
    static const uint8_t uri[] = "zclassic:t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF?amount=1.25&label=Public%20fixture&message=Hello";
    zcl_qr_image layout = {0};
    size_t image_len = 0;
    uint8_t *image = scan_fixture(uri, sizeof(uri) - 1, 4, 1, 1, 0, &layout, &image_len);
    CHECK(image != NULL);
    zcl_payment_request result = {0};
    CHECK(zcl_scan_qr(image, image_len, &layout, ZCL_MAINNET, &result) == ZCL_OK);
    CHECK(scan_result_matches(uri, sizeof(uri) - 1, ZCL_MAINNET, &result));
    CHECK(result.has_amount && result.amount == UINT64_C(125000000));
    CHECK(result.has_label && result.label_len == 14 && memcmp(result.label, "Public fixture", 14) == 0);
    CHECK(result.has_message && result.message_len == 5 && memcmp(result.message, "Hello", 5) == 0);
    free(image);
}

static void unchanged_failure(const uint8_t *image, size_t size, const zcl_qr_image *layout,
                                zcl_network network, zcl_status expected)
{
    zcl_payment_request result, before;
    memset(&result, 0xa5, sizeof(result));
    memcpy(&before, &result, sizeof(before));
    CHECK(zcl_scan_qr(image, size, layout, network, &result) == expected);
    CHECK(memcmp(&before, &result, sizeof(result)) == 0);
}

static void rejected(void)
{
    zcl_qr_image layout = {0};
    size_t image_len = 0;
    uint8_t *image = scan_fixture(address, sizeof(address) - 1, 3, 0, 1, 0, &layout, &image_len);
    CHECK(image != NULL);
    unchanged_failure(image, image_len, &layout, ZCL_TESTNET, ZCL_UNSUPPORTED);
    unchanged_failure(image, image_len - 1, &layout, ZCL_MAINNET, ZCL_OUT_OF_RANGE);
    unchanged_failure(NULL, image_len, &layout, ZCL_MAINNET, ZCL_INVALID_ARGUMENT);
    unchanged_failure(image, image_len, NULL, ZCL_MAINNET, ZCL_INVALID_ARGUMENT);
    unchanged_failure(image, image_len, &layout, (zcl_network)2, ZCL_UNSUPPORTED);
    memset(image, 255, image_len);
    unchanged_failure(image, image_len, &layout, ZCL_MAINNET, ZCL_NOT_FOUND);
    free(image);
    static const uint8_t invalid[] = "https://example.invalid/this-is-not-a-payment";
    image = scan_fixture(invalid, sizeof(invalid) - 1, 3, 0, 1, 0, &layout, &image_len);
    CHECK(image != NULL);
    zcl_payment_request result = {0};
    CHECK(zcl_scan_qr(image, image_len, &layout, ZCL_MAINNET, &result) != ZCL_OK);
    free(image);
}

static void bounds(void)
{
    zcl_qr_image layout = {1024, 1024, 8192, 4};
    CHECK(zcl_scan_image_bounds(ZCL_SCAN_INPUT_MAX, &layout) == ZCL_OK);
    CHECK(zcl_scan_image_bounds(SIZE_MAX, &layout) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_scan_image_bounds(0, &layout) == ZCL_OUT_OF_RANGE);
    const size_t invalid[] = {0, 1, 20, 1025, SIZE_MAX};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        layout.width = invalid[i];
        CHECK(zcl_scan_image_bounds(ZCL_SCAN_INPUT_MAX, &layout) == ZCL_OUT_OF_RANGE);
    }
    layout = (zcl_qr_image){21, 21, 21, 1};
    CHECK(zcl_scan_image_bounds(441, &layout) == ZCL_OK);
    layout.row_stride = 20;
    CHECK(zcl_scan_image_bounds(441, &layout) == ZCL_OUT_OF_RANGE);
    layout.row_stride = SIZE_MAX;
    CHECK(zcl_scan_image_bounds(SIZE_MAX, &layout) == ZCL_OUT_OF_RANGE);
    layout = (zcl_qr_image){21, 21, 21, SIZE_MAX};
    CHECK(zcl_scan_image_bounds(441, &layout) == ZCL_OUT_OF_RANGE);
}

int main(void)
{
    bounds();
    accepted();
    payment();
    rejected();
    puts("C scan bounds, public requests, rotations, strides and unchanged failures passed");
    return 0;
}
