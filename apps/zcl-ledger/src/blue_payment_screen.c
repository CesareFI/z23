/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_payment_screen.h"
#include "zcl_base58.h"

#include <string.h>

enum { ZCL_MAX_MONEY_ZAT = 2100000000000000ULL };

static bool append_number(char *text, size_t capacity, size_t *used,
    uint64_t value) {
    char digits[20];
    size_t count = 0;
    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    if (count >= capacity - *used) return false;
    while (count) text[(*used)++] = digits[--count];
    text[*used] = 0;
    return true;
}

static bool amount_text(uint64_t zat, char text[32]) {
    size_t used = 0;
    if (!append_number(text, 32, &used, zat / 100000000)) return false;
    text[used++] = '.';
    uint64_t fraction = zat % 100000000;
    uint64_t place = 10000000;
    for (unsigned i = 0; i < 8; ++i, place /= 10) {
        text[used++] = (char)('0' + fraction / place);
        fraction %= place;
    }
    memcpy(text + used, " ZCL", 5);
    return true;
}

static bool output_address(const blue_payment_output *output,
    blue_payment_hash_fn hash, char address[40]) {
    uint8_t payload[26] = {0x1c};
    uint8_t first[32], second[32];
    payload[1] = output->type == ZCL_TX_STREAM_P2PKH ? 0xb8 : 0xbd;
    memcpy(payload + 2, output->hash160, 20);
    if (!hash(payload, 22, first) || !hash(first, 32, second)) return false;
    memcpy(payload + 22, second, 4);
    if (zcl_base58_encode(payload, sizeof payload, address, 40) != 0 ||
        strlen(address) != 35 || address[0] != 't') return false;
    return address[1] == (output->type == ZCL_TX_STREAM_P2PKH ? '1' : '3');
}

static bool valid_output(const blue_payment_output *output,
    uint32_t total_outputs, blue_payment_hash_fn hash) {
    return output && hash && total_outputs &&
        total_outputs <= BLUE_PAYMENT_REVIEW_MAX_OUTPUTS &&
        output->index < total_outputs &&
        output->amount_zat <= ZCL_MAX_MONEY_ZAT &&
        (output->type == ZCL_TX_STREAM_P2PKH ||
         output->type == ZCL_TX_STREAM_P2SH);
}

bool blue_payment_screen_format(const blue_payment_output *output,
    uint32_t total_outputs, blue_payment_hash_fn hash,
    blue_payment_screen *screen) {
    if (!screen) return false;
    memset(screen, 0, sizeof *screen);
    if (!valid_output(output, total_outputs, hash)) return false;
    memcpy(screen->title, "OUTPUT ", 7);
    size_t used = 7;
    if (!append_number(screen->title, sizeof screen->title, &used,
            output->index + 1) || used + 1 >= sizeof screen->title)
        return false;
    screen->title[used++] = '/';
    if (!append_number(screen->title, sizeof screen->title, &used,
            total_outputs) ||
        !amount_text(output->amount_zat, screen->amount) ||
        !output_address(output, hash, screen->address)) {
        memset(screen, 0, sizeof *screen);
        return false;
    }
    memcpy(screen->address_lines[0], screen->address, 12);
    memcpy(screen->address_lines[1], screen->address + 12, 12);
    memcpy(screen->address_lines[2], screen->address + 24, 11);
    strcpy(screen->kind,
        output->type == ZCL_TX_STREAM_P2PKH ? "P2PKH" : "P2SH");
    return true;
}
