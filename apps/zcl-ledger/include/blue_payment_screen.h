/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLUE_PAYMENT_SCREEN_H
#define ZCL_BLUE_PAYMENT_SCREEN_H

#include "blue_payment_review.h"

typedef bool (*blue_payment_hash_fn)(const uint8_t *bytes, size_t length,
    uint8_t digest[32]);

typedef struct {
    char title[24];
    char amount[32];
    char address[40];
    char address_lines[3][13];
    char kind[8];
} blue_payment_screen;

/* Formats one provisional transparent output from the replay parser.
 * The full address is retained; this screen cannot approve signing. */
bool blue_payment_screen_format(const blue_payment_output *output,
    uint32_t total_outputs, blue_payment_hash_fn hash,
    blue_payment_screen *screen);

#endif
