/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_payment_render.h"
#include "blue_bagl_canvas.h"
#include "blue_wallet_layout.h"

#include <stdint.h>

static bool draw(blue_bagl_canvas *canvas, const char *label, int y,
    blue_bagl_font font, uint32_t foreground, uint32_t background) {
    return blue_bagl_text(canvas, label, 20, y, 280, true,
        foreground, background, font);
}

static bool draw_details(blue_bagl_canvas *canvas,
    const blue_payment_screen *screen, uint32_t foreground,
    uint32_t background) {
    if (!draw(canvas, screen->title, 25, BLUE_BAGL_TEXT_22,
              foreground, background) ||
        !draw(canvas, screen->kind, 68, BLUE_BAGL_TEXT_14,
              foreground, background) ||
        !draw(canvas, screen->amount, 115, BLUE_BAGL_TEXT_22,
              foreground, background) ||
        !draw(canvas, "ZCL MAINNET ADDRESS", 172, BLUE_BAGL_TEXT_14,
              foreground, background)) return false;
    for (unsigned i = 0; i < 3; ++i)
        if (!draw(canvas, screen->address_lines[i], 208 + (int)i * 36,
                  BLUE_BAGL_TEXT_22, foreground, background)) return false;
    return draw(canvas, "DRAFT; NO SIGNING", 331, BLUE_BAGL_TEXT_14,
                foreground, background);
}

bool blue_payment_render_png(const char *path,
    const blue_payment_screen *screen, bool dark) {
    if (!path || !screen) return false;
    uint32_t background = dark ? ZCL_WALLET_COLOR_BODY : 0xf5f7f8;
    uint32_t foreground = dark ? ZCL_WALLET_COLOR_TEXT : 0x10212a;
    uint32_t accent = dark ? ZCL_WALLET_COLOR_ACCENT : 0x116f61;
    blue_bagl_canvas *canvas = blue_bagl_canvas_create(background);
    if (!canvas) return false;
    bool fit = draw_details(canvas, screen, foreground, background);
    blue_bagl_round_rectangle(canvas, 20, 386, 135, 58, accent);
    blue_bagl_round_rectangle(canvas, 165, 386, 135, 58, accent);
    if (fit) fit = blue_bagl_text(canvas, "CONTINUE", 20, 404,
        135, true, dark ? background : 0xffffff, accent, BLUE_BAGL_TEXT_14);
    if (fit) fit = blue_bagl_text(canvas, "EXIT", 165, 404,
        135, true, dark ? background : 0xffffff, accent, BLUE_BAGL_TEXT_14);
    if (fit) fit = blue_bagl_write_png(canvas, path);
    blue_bagl_canvas_destroy(canvas);
    return fit;
}
