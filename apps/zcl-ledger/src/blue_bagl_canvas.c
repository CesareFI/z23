/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_bagl_canvas.h"

#include <png.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct {
    unsigned char char_width;
    unsigned char bitmap_byte_count;
    unsigned short bitmap_offset;
} bagl_font_character_t;

typedef struct {
    unsigned int font_id;
    unsigned char bpp, char_height, baseline_height, char_kerning;
    unsigned short first_char, last_char;
    const bagl_font_character_t *characters;
    const unsigned char *bitmap;
} bagl_font_t;

enum { BAGL_FONT_OPEN_SANS_LIGHT_16_22PX = 1,
       BAGL_FONT_OPEN_SANS_REGULAR_11_14PX = 4 };
#include "../third_party/ledger/bagl_font_open_sans_regular_11_14px.inc"
#include "../third_party/ledger/bagl_font_open_sans_light_16_22px.inc"

struct blue_bagl_canvas {
    uint8_t rgb[BLUE_BAGL_WIDTH * BLUE_BAGL_HEIGHT * 3];
};

static const bagl_font_t *select_font(blue_bagl_font font) {
    if (font == BLUE_BAGL_TEXT_14)
        return &fontOPEN_SANS_REGULAR_11_14PX;
    if (font == BLUE_BAGL_TEXT_22)
        return &fontOPEN_SANS_LIGHT_16_22PX;
    return NULL;
}

int blue_bagl_font_height(blue_bagl_font font) {
    const bagl_font_t *selected = select_font(font);
    return selected ? selected->char_height : 0;
}

static void pixel(blue_bagl_canvas *image, int x, int y, uint32_t color) {
    if (x < 0 || x >= BLUE_BAGL_WIDTH ||
        y < 0 || y >= BLUE_BAGL_HEIGHT) return;
    size_t offset = ((size_t)y * BLUE_BAGL_WIDTH + (size_t)x) * 3;
    image->rgb[offset] = (uint8_t)(color >> 16);
    image->rgb[offset + 1] = (uint8_t)(color >> 8);
    image->rgb[offset + 2] = (uint8_t)color;
}

blue_bagl_canvas *blue_bagl_canvas_create(uint32_t background) {
    blue_bagl_canvas *image = malloc(sizeof *image);
    if (image)
        blue_bagl_rectangle(image, 0, 0, BLUE_BAGL_WIDTH,
                            BLUE_BAGL_HEIGHT, background);
    return image;
}

void blue_bagl_canvas_destroy(blue_bagl_canvas *image) { free(image); }

void blue_bagl_rectangle(blue_bagl_canvas *image, int x, int y,
                         int width, int height, uint32_t color) {
    if (!image || width <= 0 || height <= 0) return;
    int left = x < 0 ? 0 : x;
    int top = y < 0 ? 0 : y;
    int64_t right = (int64_t)x + width;
    int64_t bottom = (int64_t)y + height;
    if (right > BLUE_BAGL_WIDTH) right = BLUE_BAGL_WIDTH;
    if (bottom > BLUE_BAGL_HEIGHT) bottom = BLUE_BAGL_HEIGHT;
    for (int row = top; row < bottom; ++row)
        for (int col = left; col < right; ++col)
            pixel(image, col, row, color);
}

void blue_bagl_round_rectangle(blue_bagl_canvas *image, int x, int top,
                               int width, int height, uint32_t color) {
    if (!image || width < 12 || height < 12 || x < 0 || top < 0 ||
        width > BLUE_BAGL_WIDTH - x ||
        height > BLUE_BAGL_HEIGHT - top) return;
    for (int y = 0; y < height; ++y)
        for (int col = 0; col < width; ++col) {
            int dx = col < 6 ? 5 - col : col >= width - 6 ?
                col - (width - 6) : 0;
            int dy = y < 6 ? 5 - y : y >= height - 6 ?
                y - (height - 6) : 0;
            if (dx * dx + dy * dy <= 36)
                pixel(image, x + col, top + y, color);
        }
}

static uint32_t blend(uint32_t background, uint32_t foreground,
                      unsigned alpha) {
    uint32_t result = 0;
    for (unsigned shift = 0; shift < 24; shift += 8) {
        unsigned bg = (background >> shift) & 255;
        unsigned fg = (foreground >> shift) & 255;
        result |= (uint32_t)((bg * (15 - alpha) + fg * alpha + 7) / 15) << shift;
    }
    return result;
}

static bool text_width(const char *label, const bagl_font_t *font,
                       int limit, int *width) {
    int sum = 0;
    for (const unsigned char *p = (const unsigned char *)label; *p; ++p) {
        if (*p < 0x20 || *p > 0x7e) return false;
        int glyph_width = font->characters[*p - 0x20].char_width;
        if (glyph_width > limit - sum) return false;
        sum += glyph_width;
    }
    *width = sum;
    return true;
}

bool blue_bagl_text(blue_bagl_canvas *image, const char *label,
                    int x, int y, int width, bool centered,
                    uint32_t foreground, uint32_t background,
                    blue_bagl_font font_choice) {
    const bagl_font_t *font = select_font(font_choice);
    int measured;
    if (!image || !label || !font || x < 0 || y < 0 || width <= 0 ||
        width > BLUE_BAGL_WIDTH - x ||
        font->char_height > BLUE_BAGL_HEIGHT - y ||
        !text_width(label, font, width, &measured)) return false;
    int cursor = x + (centered ? width / 2 - measured / 2 : 0);
    for (const unsigned char *p = (const unsigned char *)label; *p; ++p) {
        const bagl_font_character_t *glyph = &font->characters[*p - 0x20];
        const uint8_t *bitmap = font->bitmap + glyph->bitmap_offset;
        for (unsigned row = 0; row < font->char_height; ++row)
            for (unsigned col = 0; col < glyph->char_width; ++col) {
                unsigned offset = row * glyph->char_width + col;
                unsigned alpha = (bitmap[offset / 2] >> (4 * (offset % 2))) & 15;
                pixel(image, cursor + (int)col, y + (int)row,
                      blend(background, foreground, alpha));
            }
        cursor += glyph->char_width;
    }
    return true;
}

bool blue_bagl_write_png(const blue_bagl_canvas *image, const char *path) {
    if (!image || !path) return false;
    png_image png = {0};
    png.version = PNG_IMAGE_VERSION;
    png.width = BLUE_BAGL_WIDTH;
    png.height = BLUE_BAGL_HEIGHT;
    png.format = PNG_FORMAT_RGB;
    bool written = png_image_write_to_file(&png, path, 0, image->rgb,
                                            0, NULL) != 0;
    png_image_free(&png);
    return written;
}
