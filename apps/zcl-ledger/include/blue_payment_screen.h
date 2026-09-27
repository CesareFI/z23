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
    char kind[20];
} blue_payment_screen;

typedef enum {
    BLUE_PAYMENT_ACCOUNT_UNKNOWN,
    BLUE_PAYMENT_THIS_ACCOUNT,
    BLUE_PAYMENT_OTHER_P2PKH,
    BLUE_PAYMENT_P2SH_ADDRESS
} blue_payment_account_relation;

/* Only an exact P2PKH hash match to the device-derived fixed account is
 * "this account". P2SH cannot be classified as owned from its script hash. */
blue_payment_account_relation blue_payment_account_classify(
    const blue_payment_output *output,
    const uint8_t account_hash160[20], bool account_ready);

/* Replaces the output-type label with a device-derived account relation.
 * Failure clears the label so an unverified account claim cannot be shown. */
bool blue_payment_screen_mark_account(blue_payment_screen *screen,
    const blue_payment_output *output,
    const uint8_t account_hash160[20], bool account_ready);

/* Formats one provisional transparent output from the replay parser.
 * The full address is retained; this screen cannot approve signing. */
bool blue_payment_screen_format(const blue_payment_output *output,
    uint32_t total_outputs, blue_payment_hash_fn hash,
    blue_payment_screen *screen);

#endif
