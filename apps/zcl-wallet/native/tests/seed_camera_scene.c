/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* Host-only public camera fixture. Fixed PNG dimensions and stored DEFLATE
 * rows need no general codec provider. This never enters an Android library. */
#include "qrcodegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCENE_BYTES ((size_t)640 * 480 * 3)

static bool render(uint8_t *rgb, size_t capacity)
{
    if (rgb == NULL || capacity != SCENE_BYTES) return false;
    const char request[] = "zclassic:t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF?amount=1.25&label=CameraFixture";
    uint8_t temporary[qrcodegen_BUFFER_LEN_FOR_VERSION(8)] = {0};
    uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(8)] = {0};
    if (!qrcodegen_encodeText(request, temporary, qr, qrcodegen_Ecc_QUARTILE,
            1, 8, qrcodegen_Mask_AUTO, true)) return false;
    const int side = qrcodegen_getSize(qr);
    if (side <= 0 || side > 49) return false;
    memset(rgb, 255, capacity);
    /* The imagefile backend rotates and crops landscape input. Center x=180
     * keeps its full target visible. side<=49 and scale6 prove all bounds. */
    const int left = 180 - side * 3, top = (480 - side * 6) / 2;
    for (int y = 0; y < side; ++y) for (int x = 0; x < side; ++x) {
        if (!qrcodegen_getModule(qr, x, y)) continue;
        for (int dy = 0; dy < 6; ++dy) for (int dx = 0; dx < 6; ++dx) {
            const size_t offset = ((size_t)(top + y * 6 + dy) * 640 + (size_t)(left + x * 6 + dx)) * 3;
            memset(rgb + offset, 0, 3);
        }
    }
    return true;
}

static uint32_t crc_update(uint32_t crc, const uint8_t *bytes, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) != 0 ? UINT32_C(0xedb88320) : 0u);
    }
    return crc;
}

static bool write_crc(FILE *file, const uint8_t *bytes, size_t length, uint32_t *crc)
{
    if (fwrite(bytes, 1, length, file) != length) return false;
    *crc = crc_update(*crc, bytes, length);
    return true;
}

static bool write_word(FILE *file, uint32_t value, uint32_t *crc)
{
    const uint8_t bytes[4] = {(uint8_t)(value >> 24), (uint8_t)(value >> 16),
        (uint8_t)(value >> 8), (uint8_t)value};
    if (fwrite(bytes, 1, sizeof(bytes), file) != sizeof(bytes)) return false;
    if (crc != NULL) *crc = crc_update(*crc, bytes, sizeof(bytes));
    return true;
}

/* The first four bytes are the chunk type, followed by its fixed data. */
static bool write_chunk(FILE *file, const uint8_t *bytes, size_t length)
{
    if (bytes == NULL || length < 4 || length - 4 > UINT32_MAX) return false;
    uint32_t crc = UINT32_MAX;
    return write_word(file, (uint32_t)(length - 4), NULL) &&
        write_crc(file, bytes, length, &crc) && write_word(file, crc ^ UINT32_MAX, NULL);
}

static void adler_update(uint32_t *a, uint32_t *b, const uint8_t *bytes, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        *a = (*a + bytes[i]) % 65521u;
        *b = (*b + *a) % 65521u;
    }
}

static bool write_idat(FILE *file, const uint8_t *pixels, size_t length)
{
    if (pixels == NULL || length != SCENE_BYTES) return false;
    /* Each of 480 rows is one stored block: 5-byte block header, filter byte
     * zero, then 1920 RGB bytes. Zlib adds its 2-byte header and 4-byte Adler. */
    const uint8_t header[] = {'I', 'D', 'A', 'T', 0x78, 0x01};
    uint32_t crc = UINT32_MAX, a = 1, b = 0;
    if (!write_word(file, 2u + 480u * 1926u + 4u, NULL) ||
        !write_crc(file, header, sizeof(header), &crc)) return false;
    uint8_t row[1926] = {0, 0x81, 0x07, 0x7e, 0xf8, 0};
    for (size_t y = 0; y < 480; ++y) {
        row[0] = y == 479 ? 1 : 0;
        memcpy(row + 6, pixels + y * 1920, 1920);
        adler_update(&a, &b, row + 5, 1921);
        if (!write_crc(file, row, sizeof(row), &crc)) return false;
    }
    return write_word(file, (b << 16) | a, &crc) &&
        write_word(file, crc ^ UINT32_MAX, NULL);
}

static bool write_new(const char *path, const uint8_t *pixels, size_t length)
{
    if (path == NULL || pixels == NULL || length != SCENE_BYTES) return false;
    static const uint8_t signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    static const uint8_t ihdr[] = {'I', 'H', 'D', 'R', 0, 0, 2, 0x80,
        0, 0, 1, 0xe0, 8, 2, 0, 0, 0};
    static const uint8_t iend[] = {'I', 'E', 'N', 'D'};
    /* Explicit output only. Existing files are never overwritten or removed. */
    FILE *file = fopen(path, "wbx");
    if (file == NULL) return false;
    const bool written = fwrite(signature, 1, sizeof(signature), file) == sizeof(signature) &&
        write_chunk(file, ihdr, sizeof(ihdr)) && write_idat(file, pixels, length) &&
        write_chunk(file, iend, sizeof(iend));
    const int closed = fclose(file);
    return written && closed == 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) { fputs("Usage: seed_camera_scene <new-output.png>\n", stderr); return 2; }
    uint8_t *pixels = malloc(SCENE_BYTES);
    if (pixels == NULL) { fputs("Cannot allocate public camera scene\n", stderr); return 1; }
    const bool result = render(pixels, SCENE_BYTES) && write_new(argv[1], pixels, SCENE_BYTES);
    free(pixels);
    if (!result) fputs("Cannot generate public camera scene; output must be a new file\n", stderr);
    return result ? 0 : 1;
}
