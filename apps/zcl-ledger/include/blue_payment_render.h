/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLUE_PAYMENT_RENDER_H
#define ZCL_BLUE_PAYMENT_RENDER_H

#include "blue_payment_screen.h"

/* Host-only 320x480 PNG preview using the Blue SDK font bitmap. */
bool blue_payment_render_png(const char *path,
    const blue_payment_screen *screen, bool dark);

#endif
