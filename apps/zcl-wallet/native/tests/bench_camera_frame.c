/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
 * Explicit host measurement of packet packing, not camera/UI latency. Public
 * pixels only; independent packet checks bracket each fixed-size timing run. */
#define _POSIX_C_SOURCE 200809L
#include "zcl_camera.h"
#include "camera_reference.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Camera benchmark failed at %d\n", __LINE__); abort(); } } while (0)

static double elapsed_us(struct timespec start, struct timespec stop)
{
    return (double)(stop.tv_sec - start.tv_sec) * 1e6 +
        (double)(stop.tv_nsec - start.tv_nsec) / 1e3;
}

static void sample(const uint8_t *image, uint8_t *packet, zcl_qr_image layout, size_t run)
{
    const size_t length = (layout.height - 1) * layout.row_stride +
        (layout.width - 1) * layout.pixel_stride + 1;
    size_t written = 17;
    memset(packet, 0xa5, ZCL_CAMERA_PACKET_MAX + 2);
    CHECK(zcl_camera_frame_pack(image, length, &layout, packet + 1,
        ZCL_CAMERA_PACKET_MAX, &written) == ZCL_OK);
    CHECK(camera_reference_matches(image, length, &layout, ZCL_CAMERA_PACKET_MAX,
        ZCL_OK, packet, ZCL_CAMERA_PACKET_MAX + 2, written));
    struct timespec start, stop, cpu_start, cpu_stop;
    CHECK(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
    CHECK(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start) == 0);
    unsigned sum = 0;
    for (size_t i = 0; i < 2000; ++i) {
        CHECK(zcl_camera_frame_pack(image, length, &layout, packet + 1,
            ZCL_CAMERA_PACKET_MAX, &written) == ZCL_OK);
        sum += packet[6] + (unsigned)packet[written];
    }
    CHECK(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_stop) == 0);
    CHECK(clock_gettime(CLOCK_MONOTONIC, &stop) == 0);
    CHECK(camera_reference_matches(image, length, &layout, ZCL_CAMERA_PACKET_MAX,
        ZCL_OK, packet, ZCL_CAMERA_PACKET_MAX + 2, written));
    printf("run=%zu size=%zux%zu pixel=%zu row=%zu wall_us=%.6f cpu_us=%.6f checksum=%u\n",
        run, layout.width, layout.height, layout.pixel_stride, layout.row_stride,
        elapsed_us(start, stop) / 2000.0, elapsed_us(cpu_start, cpu_stop) / 2000.0, sum);
}

int main(void)
{
    /* One owner for two fixed-capacity public buffers; every ordinary exit
     * frees both. Fixture dimensions below bound arithmetic and work. */
    uint8_t *image = malloc(ZCL_SCAN_INPUT_MAX);
    uint8_t *packet = malloc(ZCL_CAMERA_PACKET_MAX + 2);
    if (image == NULL || packet == NULL) {
        free(image);
        free(packet);
        fputs("Cannot allocate bounded camera benchmark fixture\n", stderr);
        return 1;
    }
    for (size_t i = 0; i < ZCL_SCAN_INPUT_MAX; ++i)
        image[i] = (uint8_t)((i ^ (i >> 8) ^ (i >> 16)) & 255);
    const zcl_qr_image layouts[] = {
        {640, 480, 640, 1}, {640, 480, 1312, 2}, {320, 240, 320, 1},
        {320, 240, 336, 1}, {384, 384, 384, 1}, {1024, 768, 1031, 1},
        {1024, 1024, 8192, 4}, {21, 21, 21, 1}
    };
    for (size_t run = 0; run < 3; ++run)
        for (size_t i = 0; i < sizeof(layouts) / sizeof(layouts[0]); ++i)
            sample(image, packet, layouts[i], run);
    free(packet);
    free(image);
    return 0;
}
