/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLUE_PAYMENT_RENDER_H
#define ZCL_BLUE_PAYMENT_RENDER_H

#include "blue_payment_screen.h"

/* Host-only 320x480 PNG preview using the Blue SDK font bitmap. */
bool blue_payment_render_png(const char *path,
    const blue_payment_screen *screen, bool dark);

/* Mirrors the fixed-path and fee summary screen without device keys. */
bool blue_payment_render_fee_png(const char *path, uint64_t fee_zat,
    uint8_t input_paths, bool dark);

#endif
