/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "scan_fixture.h"
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

int main(void)
{
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
