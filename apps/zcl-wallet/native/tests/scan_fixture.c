/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "scan_fixture.h"
#include "qrcodegen.h"
#include <stdlib.h>
#include <string.h>

struct encoded_fixture {
    uint8_t temporary[qrcodegen_BUFFER_LEN_MAX];
    uint8_t encoded[qrcodegen_BUFFER_LEN_MAX];
};

static void paint(const uint8_t *encoded, uint8_t *image, const zcl_qr_image *layout,
                    size_t scale, unsigned int rotation)
{
    const size_t symbol = (size_t)qrcodegen_getSize(encoded);
    for (size_t y = 0; y < layout->height; ++y) {
        for (size_t x = 0; x < layout->width; ++x) {
            size_t mx = x / scale, my = y / scale;
            bool black = false;
            if (mx >= 4 && my >= 4 && mx < symbol + 4 && my < symbol + 4)
                black = qrcodegen_getModule(encoded, (int)(mx - 4), (int)(my - 4));
            size_t rx = x, ry = y;
            for (unsigned int turn = 0; turn < rotation; ++turn) {
                size_t next_x = layout->width - 1 - ry;
                ry = rx;
                rx = next_x;
            }
            image[ry * layout->row_stride + rx * layout->pixel_stride] = black ? 0 : 255;
        }
    }
}

uint8_t *scan_fixture(const uint8_t *text, size_t text_len, size_t scale,
                       unsigned int rotation, size_t pixel_stride, size_t padding,
                       zcl_qr_image *layout, size_t *image_len)
{
    if (text == NULL || layout == NULL || image_len == NULL || text_len > 1024 ||
        scale < 1 || scale > 5 || rotation > 3 || pixel_stride < 1 || pixel_stride > 4 || padding > 16)
        return NULL;
    struct encoded_fixture *work = calloc(1, sizeof(*work));
    if (work == NULL)
        return NULL;
    uint8_t *image = NULL;
    memcpy(work->temporary, text, text_len);
    if (!qrcodegen_encodeBinary(work->temporary, text_len, work->encoded, qrcodegen_Ecc_QUARTILE,
                               1, 40, qrcodegen_Mask_AUTO, true))
        goto done;
    const size_t width = ((size_t)qrcodegen_getSize(work->encoded) + 8) * scale;
    const size_t row_stride = width * pixel_stride + padding;
    /* Provider side <=177, scale <=5, stride <=4, padding <=16. */
    if (width > SIZE_MAX / row_stride)
        goto done;
    const size_t count = width * row_stride;
    image = malloc(count);
    if (image == NULL)
        goto done;
    memset(image, 0xa5, count);
    *layout = (zcl_qr_image){width, width, row_stride, pixel_stride};
    *image_len = count;
    paint(work->encoded, image, layout, scale, rotation);
done:
    free(work);
    return image;
}
