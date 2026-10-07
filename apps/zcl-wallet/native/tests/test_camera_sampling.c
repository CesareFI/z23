/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "camera_reference.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Sampling reference failed at %d\n", __LINE__); abort(); } } while (0)

static void compare(const uint8_t *image, size_t length, const zcl_qr_image *layout,
    uint8_t *guarded, size_t capacity)
{
    memset(guarded, 0xa5, ZCL_CAMERA_PACKET_MAX + 2);
    size_t written = 17;
    const zcl_status status = zcl_camera_frame_pack(image, length, layout,
        guarded + 1, capacity, &written);
    CHECK(camera_reference_matches(image, length, layout, capacity, status,
        guarded, ZCL_CAMERA_PACKET_MAX + 2, written));
    size_t measured = 17;
    const zcl_status sizing = zcl_camera_frame_size(length, layout, &measured);
    if (status == ZCL_OK) {
        CHECK(sizing == ZCL_OK && measured == written);
    } else if (status == ZCL_BUFFER_TOO_SMALL) {
        CHECK(sizing == ZCL_OK && measured > capacity && measured <= ZCL_CAMERA_PACKET_MAX);
    } else {
        CHECK(sizing == status && measured == 17);
    }
}

static void dimensions(const uint8_t *image, uint8_t *guarded)
{
    const size_t sides[] = {21, 22, 383, 384, 385, 767, 768, 769, 1023, 1024};
    for (size_t w = 0; w < sizeof(sides) / sizeof(sides[0]); ++w) {
        for (size_t h = 0; h < sizeof(sides) / sizeof(sides[0]); ++h) {
            for (size_t stride = 1; stride <= 4; ++stride) {
                const size_t row = (sides[w] - 1) * stride + 1;
                const size_t rows[] = {row, row + 7, 8192};
                for (size_t r = 0; r < sizeof(rows) / sizeof(rows[0]); ++r) {
                    const zcl_qr_image layout = {sides[w], sides[h], rows[r], stride};
                    const size_t needed = (layout.height - 1) * layout.row_stride + row;
                    compare(image, needed, &layout, guarded, ZCL_CAMERA_PACKET_MAX);
                }
            }
        }
    }
}

static void refusal_boundaries(const uint8_t *image, uint8_t *guarded)
{
    const zcl_qr_image layout = {385, 383, 1555, 4};
    size_t measured = 17;
    CHECK(zcl_camera_frame_size(ZCL_SCAN_INPUT_MAX, NULL, &measured) == ZCL_INVALID_ARGUMENT);
    CHECK(measured == 17);
    CHECK(zcl_camera_frame_size(ZCL_SCAN_INPUT_MAX, &layout, NULL) == ZCL_INVALID_ARGUMENT);
    const size_t needed = 382 * (size_t)1555 + 384 * (size_t)4 + 1;
    /* Enumerated: 193 columns and 192 rows, so a 37061-byte packet. */
    const size_t capacities[] = {0, 4, 5, 37060, 37061, 37062, ZCL_CAMERA_PACKET_MAX};
    for (size_t i = 0; i < sizeof(capacities) / sizeof(capacities[0]); ++i)
        compare(image, needed, &layout, guarded, capacities[i]);
    compare(image, needed - 1, &layout, guarded, ZCL_CAMERA_PACKET_MAX);
    compare(image, SIZE_MAX, &layout, guarded, ZCL_CAMERA_PACKET_MAX);
    const zcl_qr_image invalid[] = {{20, 21, 21, 1}, {21, 1025, 21, 1},
        {21, 21, 8193, 1}, {21, 21, 20, 1}, {21, 21, 21, 0}, {21, 21, 105, 5},
        {SIZE_MAX, 21, 21, 1}, {21, SIZE_MAX, 21, 1}, {21, 21, SIZE_MAX, 1},
        {21, 21, 21, SIZE_MAX}};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
        compare(image, ZCL_SCAN_INPUT_MAX, &invalid[i], guarded, ZCL_CAMERA_PACKET_MAX);
}

static void exact_source_spans(uint8_t *guarded)
{
    const zcl_qr_image layouts[] = {
        {21, 21, 21, 1}, {23, 27, 27, 1}, {320, 240, 320, 1},
        {384, 384, 391, 1}, {640, 480, 640, 1}, {640, 480, 1291, 2},
        {769, 385, 8192, 4}, {1024, 1024, 8192, 4}
    };
    for (size_t i = 0; i < sizeof(layouts) / sizeof(layouts[0]); ++i) {
        const zcl_qr_image *layout = &layouts[i];
        /* Fixed public layouts bound every product below 8 MiB. Unlike the
         * large shared fixture, these allocations end at the last pixel so
         * sanitizers also observe reads beyond the final valid pixel. */
        const size_t length = (layout->height - 1) * layout->row_stride +
            (layout->width - 1) * layout->pixel_stride + 1;
        uint8_t *image = malloc(length);
        CHECK(image != NULL);
        for (size_t n = 0; n < length; ++n)
            image[n] = (uint8_t)((n ^ (n >> 8) ^ (n >> 16)) & 255);
        compare(image, length, layout, guarded, ZCL_CAMERA_PACKET_MAX);
        free(image);
    }
}

static void reference_refuses_corruption(const uint8_t *image, uint8_t *guarded)
{
    const zcl_qr_image layout = {21, 21, 21, 1};
    const size_t length = 446, span = ZCL_CAMERA_PACKET_MAX + 2;
    compare(image, 441, &layout, guarded, ZCL_CAMERA_PACKET_MAX);
    /* Every prefix/header/pixel byte, plus near and distant unused tail bytes.
     * The reference must itself reject corruption, not merely accept the core. */
    for (size_t i = 0; i <= length; ++i) {
        guarded[i] ^= 1;
        CHECK(!camera_reference_matches(image, 441, &layout, ZCL_CAMERA_PACKET_MAX,
            ZCL_OK, guarded, span, length));
        guarded[i] ^= 1;
    }
    const size_t tails[] = {length + 1, length + 2, ZCL_CAMERA_PACKET_MAX / 2, span - 1};
    for (size_t i = 0; i < sizeof(tails) / sizeof(tails[0]); ++i) {
        guarded[tails[i]] ^= 1;
        CHECK(!camera_reference_matches(image, 441, &layout, ZCL_CAMERA_PACKET_MAX,
            ZCL_OK, guarded, span, length));
        guarded[tails[i]] ^= 1;
    }
    CHECK(!camera_reference_matches(image, 441, &layout, length - 1,
        ZCL_OK, guarded, span, length));
    CHECK(!camera_reference_matches(image, 441, &layout, ZCL_CAMERA_PACKET_MAX,
        ZCL_OK, guarded, span, SIZE_MAX));
    CHECK(!camera_reference_matches(image, 441, &layout, ZCL_CAMERA_PACKET_MAX,
        ZCL_OK, guarded, span, length + 1));
    CHECK(!camera_reference_matches(image, 441, &layout, ZCL_CAMERA_PACKET_MAX,
        ZCL_OUT_OF_RANGE, guarded, span, length));
}

int main(void)
{
    /* Two constant checked fixture allocations, one owner and one free each.
     * Public coordinate markers only; no OS camera or decoder is opened. */
    uint8_t *image = malloc(ZCL_SCAN_INPUT_MAX);
    uint8_t *guarded = malloc(ZCL_CAMERA_PACKET_MAX + 2);
    if (image == NULL || guarded == NULL) {
        free(image);
        free(guarded);
        fputs("Cannot allocate bounded public sampling fixture\n", stderr);
        return 1;
    }
    for (size_t i = 0; i < ZCL_SCAN_INPUT_MAX; ++i)
        image[i] = (uint8_t)((i ^ (i >> 8) ^ (i >> 16)) & 255);
    dimensions(image, guarded);
    refusal_boundaries(image, guarded);
    exact_source_spans(guarded);
    reference_refuses_corruption(image, guarded);
    free(guarded);
    free(image);
    puts("Independent camera sampling: 1200 layouts, 8 exact spans, exact pixels and full output guards passed");
    return 0;
}
