/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_WALLET_PAYMENT_DEVICE_H
#define ZCL_WALLET_PAYMENT_DEVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "os.h"

typedef struct {
    uint8_t raw[32], chain[32];
    cx_ecfp_private_key_t key;
    cx_ecfp_public_key_t public_key;
} wallet_boot_material;

wallet_boot_material *wallet_payment_boot_material(void);
void wallet_payment_boot_clear(void);

/* Called only after device-side key derivation and receive-address formatting. */
void wallet_payment_set_account_hashes(const uint8_t external_hash160[20],
                                       const uint8_t internal_hash160[20]);

uint16_t wallet_payment_command(const uint8_t *apdu, size_t length,
    uint8_t *reply, size_t capacity, size_t *reply_length);
void wallet_payment_display(void);
bool wallet_payment_visible(void);
void wallet_payment_abort(void);
/* Ends an unconsumed final approval on the 30-second UX ticker callback. */
bool wallet_payment_timeout(void);

#endif
