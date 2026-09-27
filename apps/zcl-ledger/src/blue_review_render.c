/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_review_render.h"
#include "blue_review_accessible.h"
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

static void rounded_button(canvas *image, int x, int top,
                           int width, int height, uint32_t color) {
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
    char wrapped[40];
    if (!blue_review_accessible_wrap(label, wrapped)) return false;
    int y = ZCL_BLUE_LARGE_DETAIL_Y;
    for (char *row = wrapped; *row;) {
        char *end = row;
        while (*end && *end != '\n') ++end;
        bool more = *end == '\n';
        *end = 0;
        if (y > 330 || !draw_text(image, row, 20, y, 280, false,
                                  foreground, background, font)) return false;
        if (!more) break;
        row = end + 1;
        y += font->char_height;
    }
    return true;
}

static bool render_header(canvas *image, bool dark, uint32_t button) {
    rectangle(image, 0, 0, ZCL_BLUE_SCREEN_WIDTH, ZCL_BLUE_HEADER_HEIGHT,
              ZCL_BLUE_COLOR_HEADER);
    bool fit = draw_text(image, "ZCL Review", ZCL_BLUE_HEADER_TEXT_X,
                         (ZCL_BLUE_HEADER_HEIGHT / 2 - 12 / 2 - 1),
                         ZCL_BLUE_HEADER_TEXT_WIDTH, false,
                         ZCL_BLUE_COLOR_WHITE, ZCL_BLUE_COLOR_HEADER,
                         &fontOPEN_SANS_REGULAR_11_14PX);
    rounded_button(image, ZCL_BLUE_THEME_X, ZCL_BLUE_THEME_Y,
                   ZCL_BLUE_THEME_WIDTH, ZCL_BLUE_THEME_HEIGHT, button);
    if (fit) fit = draw_text(image, dark ? "LIGHT" : "DARK",
        ZCL_BLUE_THEME_X,
        ZCL_BLUE_THEME_Y + ZCL_BLUE_THEME_HEIGHT / 2 - 12 / 2 - 1,
        ZCL_BLUE_THEME_WIDTH, true, ZCL_BLUE_COLOR_HEADER, button,
        &fontOPEN_SANS_REGULAR_11_14PX);
    return fit;
}

static bool render_body(canvas *image, const blue_review_app *app,
    int large_line, uint32_t foreground, uint32_t background) {
    if (large_line >= 0 && large_line < ZCL_BLUE_REVIEW_LINES) {
        char title[24];
        return snprintf(title, sizeof title, "DETAIL %d/6", large_line + 1) > 0 &&
            draw_text(image, title, 20, ZCL_BLUE_LARGE_TITLE_Y, 280, true,
                      foreground, background,
                      &fontOPEN_SANS_LIGHT_16_22PX) &&
            draw_large_line(image, app->lines[large_line],
                            foreground, background);
    }
    if (large_line != -1) return false;
    for (int i = 0; i < ZCL_BLUE_REVIEW_LINES; ++i)
        if (!draw_text(image, app->lines[i], ZCL_BLUE_LINE_X,
                       ZCL_BLUE_LINE_FIRST_Y + i * ZCL_BLUE_LINE_STEP_Y,
                       ZCL_BLUE_LINE_WIDTH, true, foreground, background,
                       &fontOPEN_SANS_REGULAR_11_14PX)) return false;
    return true;
}

static bool render_controls(canvas *image, int large_line,
                            uint32_t background, uint32_t button) {
    rounded_button(image, ZCL_BLUE_NEXT_X, ZCL_BLUE_BUTTON_Y,
                   ZCL_BLUE_BUTTON_WIDTH, ZCL_BLUE_BUTTON_HEIGHT, button);
    rounded_button(image, ZCL_BLUE_EXIT_X, ZCL_BLUE_BUTTON_Y,
                   ZCL_BLUE_BUTTON_WIDTH, ZCL_BLUE_BUTTON_HEIGHT, button);
    rounded_button(image, 20, ZCL_BLUE_TEXT_TOGGLE_Y, 280,
                   ZCL_BLUE_TEXT_TOGGLE_HEIGHT, button);
    bool fit = draw_text(image,
        large_line < 0 ? "LARGER TEXT" : "STANDARD TEXT", 20,
        ZCL_BLUE_TEXT_TOGGLE_Y + ZCL_BLUE_TEXT_TOGGLE_HEIGHT / 2 - 12 / 2 - 1,
        280, true, background, button,
        &fontOPEN_SANS_REGULAR_11_14PX);
    int button_text_y = ZCL_BLUE_BUTTON_Y +
                        ZCL_BLUE_BUTTON_HEIGHT / 2 - 12 / 2 - 1;
    if (fit) fit = draw_text(image, large_line < 0 ? "NEXT PAGE" : "NEXT DETAIL",
                             ZCL_BLUE_NEXT_X, button_text_y,
                             ZCL_BLUE_BUTTON_WIDTH, true, background, button,
                             &fontOPEN_SANS_REGULAR_11_14PX);
    if (fit) fit = draw_text(image, "EXIT", ZCL_BLUE_EXIT_X, button_text_y,
                             ZCL_BLUE_BUTTON_WIDTH, true, background, button,
                             &fontOPEN_SANS_REGULAR_11_14PX);
    return fit;
}

static bool write_png(const char *path, const canvas *image) {
    png_image png = {0};
    png.version = PNG_IMAGE_VERSION;
    png.width = ZCL_BLUE_SCREEN_WIDTH;
    png.height = ZCL_BLUE_SCREEN_HEIGHT;
    png.format = PNG_FORMAT_RGB;
    bool written = png_image_write_to_file(&png, path, 0, image->rgb,
                                            0, NULL) != 0;
    png_image_free(&png);
    return written;
}

bool blue_review_render_preview_png(const char *path,
    const blue_review_app *app, bool dark, int large_line) {
    if (!path || !app) return false;
    canvas *image = malloc(sizeof *image);
    if (!image) return false;
    uint32_t background = dark ? ZCL_BLUE_COLOR_DARK_BODY : ZCL_BLUE_COLOR_BODY;
    uint32_t foreground = dark ? ZCL_BLUE_COLOR_DARK_TEXT : ZCL_BLUE_COLOR_TEXT;
    uint32_t button = dark ? ZCL_BLUE_COLOR_DARK_BUTTON : ZCL_BLUE_COLOR_BUTTON;
    rectangle(image, 0, 0, ZCL_BLUE_SCREEN_WIDTH, ZCL_BLUE_SCREEN_HEIGHT,
              background);
    bool fit = render_header(image, dark, button) &&
               render_body(image, app, large_line, foreground, background) &&
               render_controls(image, large_line, background, button);
    if (fit) fit = write_png(path, image);
    free(image);
    return fit;
}
