/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_WALLET_PAYMENT_DEVICE_H
#define ZCL_WALLET_PAYMENT_DEVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint16_t wallet_payment_command(const uint8_t *apdu, size_t length,
    uint8_t *reply, size_t capacity, size_t *reply_length);
void wallet_payment_display(void);
bool wallet_payment_visible(void);
void wallet_payment_abort(void);

#endif
