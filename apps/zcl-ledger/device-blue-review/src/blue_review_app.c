/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_review_app.h"

#include <string.h>

void blue_review_app_reset(blue_review_app *app) {
    app->page = 0;
    memset(app->reply, 0, sizeof app->reply);
    strcpy(app->lines[0], "CONNECT Z23");
    strcpy(app->lines[1], "SEND A TRANSACTION");
    strcpy(app->lines[2], "TAP NEXT PAGE TO VIEW");
    app->lines[3][0] = 0;
    strcpy(app->lines[4], "READ ONLY; NO SIGNING");
    app->lines[5][0] = 0;
}

uint16_t blue_review_app_command(blue_review_app *app,
    const uint8_t *apdu, size_t apdu_length, uint8_t *reply,
    size_t reply_capacity, size_t *reply_length,
    blue_review_digest_fn digest, const zcl_zip243_hasher *zip243_hasher) {
    if (!app || !apdu || !reply || !reply_length) return 0x6a80;
    uint8_t instruction = apdu_length >= 2 ? apdu[1] : 0;
    uint16_t status = blue_review_handle(&app->transaction, apdu,
        apdu_length, reply, reply_capacity, reply_length, digest,
        zip243_hasher);
    if (instruction == 0x10 || instruction == 0x13 ||
        (instruction == 0x11 && status != 0x9000))
        blue_review_app_reset(app);
    if (instruction != 0x12) return status;
    if (status != 0x9000 || *reply_length != sizeof app->reply ||
        !blue_review_screen_format(reply, app->lines)) {
        blue_review_app_reset(app);
        if (status == 0x9000) status = 0x6a80;
        *reply_length = 0;
        return status;
    }
    memcpy(app->reply, reply, sizeof app->reply);
    app->page = 0;
    return status;
}

bool blue_review_app_next(blue_review_app *app, blue_review_hash_fn hash) {
    if (!app || !hash) return false;
    if (app->transaction.expected || !app->transaction.reviewed_length) {
        blue_review_app_reset(app);
        return false;
    }
    uint32_t count = (uint32_t)app->reply[4] |
        ((uint32_t)app->reply[5] << 8) |
        ((uint32_t)app->reply[6] << 16) |
        ((uint32_t)app->reply[7] << 24);
    uint32_t next = app->page;
    bool formatted = next == 0 ?
        blue_review_screen_format(app->reply, app->lines) :
        blue_review_screen_output(app->transaction.wire,
            app->transaction.reviewed_length, next - 1, hash, app->lines);
    if (!formatted) {
        blue_review_app_reset(app);
        app->transaction.reviewed_length = 0;
        return false;
    }
    app->page = next >= count ? 0 : next + 1;
    return true;
}
