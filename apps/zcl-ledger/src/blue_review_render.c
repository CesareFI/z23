/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_review_render.h"
#include "blue_review_layout.h"

#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct { uint8_t rgb[ZCL_BLUE_SCREEN_WIDTH * ZCL_BLUE_SCREEN_HEIGHT * 3]; } canvas;

static void pixel(canvas *image, int x, int y, uint32_t color) {
    if (x < 0 || x >= ZCL_BLUE_SCREEN_WIDTH ||
        y < 0 || y >= ZCL_BLUE_SCREEN_HEIGHT) return;
    size_t offset = ((size_t)y * ZCL_BLUE_SCREEN_WIDTH + (size_t)x) * 3;
    image->rgb[offset] = (uint8_t)(color >> 16);
    image->rgb[offset + 1] = (uint8_t)(color >> 8);
    image->rgb[offset + 2] = (uint8_t)color;
}

static void rectangle(canvas *image, int x, int y, int width, int height,
                      uint32_t color) {
    for (int row = y; row < y + height; ++row)
        for (int col = x; col < x + width; ++col) pixel(image, col, row, color);
}

static void rounded_button(canvas *image, int x, uint32_t color) {
    for (int y = 0; y < ZCL_BLUE_BUTTON_HEIGHT; ++y)
        for (int col = 0; col < ZCL_BLUE_BUTTON_WIDTH; ++col) {
            int dx = col < 6 ? 5 - col : col >= ZCL_BLUE_BUTTON_WIDTH - 6 ?
                col - (ZCL_BLUE_BUTTON_WIDTH - 6) : 0;
            int dy = y < 6 ? 5 - y : y >= ZCL_BLUE_BUTTON_HEIGHT - 6 ?
                y - (ZCL_BLUE_BUTTON_HEIGHT - 6) : 0;
            if (dx * dx + dy * dy <= 36)
                pixel(image, x + col, ZCL_BLUE_BUTTON_Y + y,
                      color);
        }
}

static uint32_t blend(uint32_t background, uint32_t foreground, unsigned alpha) {
    uint32_t result = 0;
    for (unsigned shift = 0; shift < 24; shift += 8) {
        unsigned bg = (background >> shift) & 255;
        unsigned fg = (foreground >> shift) & 255;
        result |= (uint32_t)((bg * (15 - alpha) + fg * alpha + 7) / 15) << shift;
    }
    return result;
}

static bool text_width(const char *label, const bagl_font_t *font, int *width) {
    int sum = 0;
    for (const unsigned char *p = (const unsigned char *)label; *p; ++p) {
        if (*p < 0x20 || *p > 0x7e) return false;
        sum += font->characters[*p - 0x20].char_width;
    }
    *width = sum;
    return true;
}

static bool draw_text(canvas *image, const char *label, int x, int y,
                      int width, bool centered, uint32_t fg, uint32_t bg,
                      const bagl_font_t *font) {
    int measured;
    if (!text_width(label, font, &measured) || measured > width) return false;
    int cursor = x + (centered ? width / 2 - measured / 2 : 0);
    for (const unsigned char *p = (const unsigned char *)label; *p; ++p) {
        const bagl_font_character_t *glyph =
            &font->characters[*p - 0x20];
        const uint8_t *bitmap = font->bitmap + glyph->bitmap_offset;
        for (unsigned row = 0; row < font->char_height; ++row)
            for (unsigned col = 0; col < glyph->char_width; ++col) {
                unsigned offset = row * glyph->char_width + col;
                unsigned alpha = (bitmap[offset / 2] >> (4 * (offset % 2))) & 15;
                pixel(image, cursor + (int)col, y + (int)row,
                      blend(bg, fg, alpha));
            }
        cursor += glyph->char_width;
    }
    return true;
}

static bool draw_large_line(canvas *image, const char *label,
                            uint32_t foreground, uint32_t background) {
    const bagl_font_t *font = &fontOPEN_SANS_LIGHT_16_22PX;
    char row[ZCL_BLUE_REVIEW_LINE_SIZE];
    size_t used = 0;
    int y = 130;
    for (const char *p = label; *p;) {
        while (*p == ' ') ++p;
        if (!*p) break;
        const char *end = p;
        while (*end && *end != ' ') ++end;
        size_t word = (size_t)(end - p);
        if (used + word + 2 > sizeof row) return false;
        size_t start = used;
        if (used) row[used++] = ' ';
        memcpy(row + used, p, word);
        used += word;
        row[used] = 0;
        int width;
        if (!text_width(row, font, &width)) return false;
        if (width <= 280) { p = end; continue; }
        if (!start) {
            used = 0;
            for (const char *part = p; part < end; ++part) {
                row[used] = *part;
                row[used + 1] = 0;
                if (!text_width(row, font, &width)) return false;
                if (width > 280) break;
                ++used;
            }
            row[used] = 0;
            if (!used || y > 305 ||
                !draw_text(image, row, 20, y, 280, true,
                           foreground, background, font)) return false;
            y += 42;
            p += used;
            used = 0;
            continue;
        }
        if (y > 305) return false;
        row[start] = 0;
        if (!draw_text(image, row, 20, y, 280, true,
                       foreground, background, font)) return false;
        y += 42;
        used = 0;
    }
    return !used || (y <= 305 && draw_text(image, row, 20, y, 280, true,
                                           foreground, background, font));
}

bool blue_review_render_preview_png(const char *path,
    const blue_review_app *app, bool dark, int large_line) {
    if (!path || !app) return false;
    canvas *image = malloc(sizeof *image);
    if (!image) return false;
    uint32_t background = dark ? 0x13171e : ZCL_BLUE_COLOR_BODY;
    uint32_t foreground = dark ? 0xf5f7f8 : ZCL_BLUE_COLOR_TEXT;
    uint32_t button = dark ? 0x49d9c1 : ZCL_BLUE_COLOR_BUTTON;
    rectangle(image, 0, 0, ZCL_BLUE_SCREEN_WIDTH, ZCL_BLUE_SCREEN_HEIGHT,
              background);
    rectangle(image, 0, 0, ZCL_BLUE_SCREEN_WIDTH, ZCL_BLUE_HEADER_HEIGHT,
              ZCL_BLUE_COLOR_HEADER);
    bool fit = draw_text(image, "ZCL Review", ZCL_BLUE_HEADER_TEXT_X,
                         (ZCL_BLUE_HEADER_HEIGHT / 2 - 12 / 2 - 1),
                         ZCL_BLUE_HEADER_TEXT_WIDTH, false,
                         ZCL_BLUE_COLOR_WHITE, ZCL_BLUE_COLOR_HEADER,
                         &fontOPEN_SANS_REGULAR_11_14PX);
    if (large_line >= 0 && large_line < ZCL_BLUE_REVIEW_LINES) {
        char title[24];
        if (snprintf(title, sizeof title, "DETAIL %d/6", large_line + 1) < 0)
            fit = false;
        if (fit) fit = draw_text(image, title, 20, 80, 280, true,
                                foreground, background,
                                &fontOPEN_SANS_LIGHT_16_22PX);
        if (fit) fit = draw_large_line(image, app->lines[large_line],
                                       foreground, background);
    } else if (large_line == -1) {
        for (int i = 0; i < ZCL_BLUE_REVIEW_LINES && fit; ++i)
            fit = draw_text(image, app->lines[i], ZCL_BLUE_LINE_X,
                            ZCL_BLUE_LINE_FIRST_Y + i * ZCL_BLUE_LINE_STEP_Y,
                            ZCL_BLUE_LINE_WIDTH, true, foreground, background,
                            &fontOPEN_SANS_REGULAR_11_14PX);
    } else fit = false;
    rounded_button(image, ZCL_BLUE_NEXT_X, button);
    rounded_button(image, ZCL_BLUE_EXIT_X, button);
    int button_text_y = ZCL_BLUE_BUTTON_Y +
                        ZCL_BLUE_BUTTON_HEIGHT / 2 - 12 / 2 - 1;
    if (fit) fit = draw_text(image, large_line < 0 ? "NEXT PAGE" : "NEXT DETAIL",
                             ZCL_BLUE_NEXT_X, button_text_y,
                             ZCL_BLUE_BUTTON_WIDTH, true, background, button,
                             &fontOPEN_SANS_REGULAR_11_14PX);
    if (fit) fit = draw_text(image, "EXIT", ZCL_BLUE_EXIT_X, button_text_y,
                             ZCL_BLUE_BUTTON_WIDTH, true, background, button,
                             &fontOPEN_SANS_REGULAR_11_14PX);
    if (fit) {
        png_image png = {0};
        png.version = PNG_IMAGE_VERSION;
        png.width = ZCL_BLUE_SCREEN_WIDTH;
        png.height = ZCL_BLUE_SCREEN_HEIGHT;
        png.format = PNG_FORMAT_RGB;
        fit = png_image_write_to_file(&png, path, 0, image->rgb, 0, NULL) != 0;
        png_image_free(&png);
    }
    free(image);
    return fit;
}
