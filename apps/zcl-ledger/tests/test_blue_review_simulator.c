/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_review_app.h"
#include "zcl_zip243_host.h"

#undef NDEBUG
#include <assert.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <string.h>

static bool sha256(const uint8_t *bytes, size_t length, uint8_t out[32]) {
    return SHA256(bytes, length, out) != NULL;
}

static int digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

static size_t read_fixture(const char *path, uint8_t wire[245]) {
    FILE *file = fopen(path, "r");
    assert(file);
    char line[512];
    do { assert(fgets(line, sizeof line, file)); } while (line[0] == '#');
    assert(strcspn(line, "\r\n") == 490);
    for (size_t i = 0; i < 245; ++i) {
        int high = digit(line[i * 2]), low = digit(line[i * 2 + 1]);
        assert(high >= 0 && low >= 0);
        wire[i] = (uint8_t)((high << 4) | low);
    }
    assert(!ferror(file) && fclose(file) == 0);
    return 245;
}

static uint16_t command(blue_review_app *app, uint8_t apdu[260],
                        size_t length, size_t *reply_length) {
    struct blake2b_ctx context;
    zcl_zip243_hasher hasher = zcl_zip243_host_hasher(&context);
    return blue_review_app_command(app, apdu, length, apdu, 255,
                                   reply_length, sha256, &hasher);
}

static void expect_screen(const blue_review_app *app, const char *first) {
    assert(strcmp(app->lines[0], first) == 0);
    for (size_t i = 0; i < ZCL_BLUE_REVIEW_LINES; ++i)
        printf("%s\n", app->lines[i]);
    puts("");
}

static void simulate(const uint8_t *wire, size_t length) {
    blue_review_app app = {0};
    uint8_t apdu[260] = {0xa5, 0x01, 0, 0, 0};
    size_t reply_length = 0;
    blue_review_app_reset(&app);
    expect_screen(&app, "CONNECT Z23");
    assert(!blue_review_app_next(&app, sha256));
    assert(command(&app, apdu, 5, &reply_length) == 0x9000);
    assert(reply_length == 5 && memcmp(apdu, "ZCL\x06\x40", 5) == 0);
    memcpy(apdu, (uint8_t[]){0xa5, 0x10, 0, 0, 2,
                            (uint8_t)length, (uint8_t)(length >> 8)}, 7);
    assert(command(&app, apdu, 7, &reply_length) == 0x9000);
    for (size_t offset = 0; offset < length; offset += 220) {
        size_t count = length - offset < 220 ? length - offset : 220;
        apdu[0] = 0xa5;
        apdu[1] = 0x11;
        apdu[2] = apdu[3] = 0;
        apdu[4] = (uint8_t)count;
        memcpy(apdu + 5, wire + offset, count);
        assert(command(&app, apdu, 5 + count, &reply_length) == 0x9000);
    }
    assert(!blue_review_app_next(&app, sha256));
    expect_screen(&app, "CONNECT Z23");
    memcpy(apdu, (uint8_t[]){0xa5, 0x14, 0, 0, 4,
                            0xbb, 0x09, 0xb8, 0x76}, 9);
    assert(command(&app, apdu, 9, &reply_length) == 0x9000);
    assert(reply_length == 32);
    memcpy(apdu, (uint8_t[]){0xa5, 0x12, 0, 0, 0}, 5);
    assert(command(&app, apdu, 5, &reply_length) == 0x9000);
    assert(reply_length == 76 && app.transaction.reviewed_length == length);
    assert(blue_review_app_next(&app, sha256));
    expect_screen(&app, "PUBLIC IN/OUT: 1/2");
    assert(blue_review_app_next(&app, sha256));
    expect_screen(&app, "OUTPUT 1/2: P2PKH");
    assert(blue_review_app_next(&app, sha256));
    expect_screen(&app, "OUTPUT 2/2: P2PKH");
    assert(blue_review_app_next(&app, sha256));
    expect_screen(&app, "PUBLIC IN/OUT: 1/2");
    memcpy(apdu, (uint8_t[]){0xa5, 0x13, 0, 0, 0}, 5);
    assert(command(&app, apdu, 5, &reply_length) == 0x9000);
    expect_screen(&app, "CONNECT Z23");
    assert(!blue_review_app_next(&app, sha256));
}

int main(int argc, char **argv) {
    assert(argc == 2);
    uint8_t wire[245];
    size_t length = read_fixture(argv[1], wire);
    simulate(wire, length);
    return 0;
}
