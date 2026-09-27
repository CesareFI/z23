/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_payment_render.h"
#include "zcl_address.h"

#include <openssl/sha.h>

#undef NDEBUG
#include <assert.h>
#include <string.h>

static bool digest(const uint8_t *bytes, size_t length, uint8_t output[32]) {
    return SHA256(bytes, length, output) != NULL;
}

static bool reject_hash(const uint8_t *bytes, size_t length,
    uint8_t output[32]) {
    (void)bytes;
    (void)length;
    (void)output;
    return false;
}

static void check_address(const blue_payment_screen *screen,
    const char *expected) {
    char combined[40];
    strcpy(combined, screen->address_lines[0]);
    strcat(combined, screen->address_lines[1]);
    strcat(combined, screen->address_lines[2]);
    assert(strlen(combined) == 35);
    assert(strcmp(combined, screen->address) == 0);
    assert(strcmp(combined, expected) == 0);
}

int main(int argc, char **argv) {
    blue_payment_output output = {.index = 0,
        .amount_zat = 100000001, .type = ZCL_TX_STREAM_P2PKH};
    memset(output.hash160, 0x11, 20);
    blue_payment_screen screen;
    char expected[ZCL_ADDRESS_SIZE];
    assert(zcl_address_from_hash160(output.hash160, false, expected) == 0);
    assert(blue_payment_screen_format(&output, 2, digest, &screen));
    assert(strcmp(screen.title, "OUTPUT 1/2") == 0);
    assert(strcmp(screen.amount, "1.00000001 ZCL") == 0);
    assert(strcmp(screen.kind, "P2PKH") == 0);
    check_address(&screen, expected);
    if (argc == 3) assert(blue_payment_render_png(argv[1], &screen, false));

    output.index = 1;
    output.amount_zat = 2100000000000000ULL;
    output.type = ZCL_TX_STREAM_P2SH;
    memset(output.hash160, 0x22, 20);
    assert(zcl_address_from_hash160(output.hash160, true, expected) == 0);
    assert(blue_payment_screen_format(&output, 2, digest, &screen));
    assert(strcmp(screen.title, "OUTPUT 2/2") == 0);
    assert(strcmp(screen.amount, "21000000.00000000 ZCL") == 0);
    assert(strcmp(screen.kind, "P2SH") == 0);
    check_address(&screen, expected);
    if (argc == 3) assert(blue_payment_render_png(argv[2], &screen, true));

    assert(!blue_payment_screen_format(&output, 0, digest, &screen));
    assert(!blue_payment_screen_format(&output, 17, digest, &screen));
    assert(!blue_payment_screen_format(&output, 1, digest, &screen));
    output.amount_zat++;
    assert(!blue_payment_screen_format(&output, 2, digest, &screen));
    output.amount_zat--;
    assert(!blue_payment_screen_format(&output, 2, reject_hash, &screen));
    assert(screen.title[0] == 0 && screen.address[0] == 0);
    assert(!blue_payment_screen_format(NULL, 2, digest, &screen));
    assert(!blue_payment_render_png(NULL, &screen, false));
    return 0;
}
