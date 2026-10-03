/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "scan_fixture.h"
#include "quirc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Scan ownership check failed at %d\n", __LINE__); abort(); } } while (0)
void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void __real_free(void *pointer);
void *__wrap_malloc(size_t size);
void *__wrap_calloc(size_t count, size_t size);
void __wrap_free(void *pointer);
static bool active;
static size_t calls, fail_at, live;
static void *owned[8];
static size_t owned_size[8];

static void track(void *pointer, size_t size)
{
    if (pointer == NULL) return;
    CHECK(live < 8);
    for (size_t i = 0; i < 8; ++i) {
        if (owned[i] == NULL) {
            owned[i] = pointer;
            owned_size[i] = size;
            ++live;
            return;
        }
    }
    abort();
}

void *__wrap_malloc(size_t size)
{
    if (!active) return __real_malloc(size);
    CHECK(size > 0 && size <= ZCL_SCAN_INPUT_MAX);
    if (++calls == fail_at) return NULL;
    void *pointer = __real_malloc(size);
    track(pointer, size);
    return pointer;
}

void *__wrap_calloc(size_t count, size_t size)
{
    if (!active) return __real_calloc(count, size);
    CHECK(size > 0 && count > 0 && count <= SIZE_MAX / size);
    CHECK(count * size <= ZCL_SCAN_INPUT_MAX);
    if (++calls == fail_at) return NULL;
    void *pointer = __real_calloc(count, size);
    track(pointer, count * size);
    return pointer;
}

void __wrap_free(void *pointer)
{
    if (!active || pointer == NULL) { __real_free(pointer); return; }
    for (size_t i = 0; i < 8; ++i) {
        if (owned[i] == pointer) {
            const uint8_t *bytes = pointer;
            for (size_t byte = 0; byte < owned_size[i]; ++byte)
                CHECK(bytes[byte] == 0);
            owned[i] = NULL;
            CHECK(live > 0);
            --live;
            __real_free(pointer);
            return;
        }
    }
    abort(); /* Unowned pointer or second free. */
}

static void check_retained_frame(struct quirc *decoder, int expected_width,
    int expected_height, uintptr_t previous, bool failed)
{
    int width = 0, height = 0;
    const uint8_t *pixels = quirc_begin(decoder, &width, &height);
    CHECK(pixels != NULL && width == expected_width && height == expected_height);
    if (failed) CHECK((uintptr_t)pixels == previous);
    const size_t length = (size_t)width * (size_t)height; /* Checked fixture dimensions <=80. */
    for (size_t i = 0; i < length; ++i) CHECK(pixels[i] == (i < 64 * 64 ? 0x42 : 0));
}

static void reused_resize(int width, int height, size_t failure)
{
    CHECK(!active && live == 0);
    active = true;
    fail_at = SIZE_MAX;
    calls = 0;
    struct quirc *decoder = quirc_new();
    CHECK(decoder != NULL && quirc_resize(decoder, 64, 64) == 0 && live == 3);
    uint8_t *pixels = quirc_begin(decoder, NULL, NULL);
    CHECK(pixels != NULL);
    memset(pixels, 0x42, 64 * 64);
    const uintptr_t previous = (uintptr_t)pixels;
    pixels = NULL; /* A successful resize may retire this frame. */
    calls = 0;
    fail_at = failure;
    const int status = quirc_resize(decoder, width, height);
    const bool failed = failure <= 2; /* Pinned provider aliases pixels to image. */
    CHECK(status == (failed ? -1 : 0) && calls == (failed ? failure : 2) && live == 3);
    check_retained_frame(decoder, failed ? 64 : width, failed ? 64 : height, previous, failed);
    quirc_destroy(decoder);
    CHECK(live == 0);
    active = false;
}

int main(void)
{
    const int dimensions[] = {21, 64, 80};
    for (size_t i = 0; i < sizeof(dimensions) / sizeof(dimensions[0]); ++i)
        for (size_t failure = 1; failure <= 3; ++failure)
            reused_resize(dimensions[i], dimensions[i], failure);
    static const uint8_t address[] = "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF";
    zcl_qr_image layout = {0};
    size_t size = 0;
    uint8_t *image = scan_fixture(address, sizeof(address) - 1, 3, 0, 1, 0, &layout, &size);
    CHECK(image != NULL);
    for (fail_at = 1; fail_at <= 5; ++fail_at) {
        zcl_payment_request output, before;
        memset(&output, 0xa5, sizeof(output));
        memcpy(&before, &output, sizeof(before));
        calls = 0;
        active = true;
        const zcl_status status = zcl_scan_qr(image, size, &layout, ZCL_MAINNET, &output);
        active = false;
        CHECK(live == 0);
        if (fail_at <= 4) {
            CHECK(status == ZCL_RESOURCE_EXHAUSTED);
            CHECK(memcmp(&output, &before, sizeof(output)) == 0);
        } else {
            CHECK(status == ZCL_OK && calls == 4);
        }
    }
    free(image);
    puts("Every scanner allocation failure preserves output; owned buffers are cleared and freed exactly once");
    return 0;
}
