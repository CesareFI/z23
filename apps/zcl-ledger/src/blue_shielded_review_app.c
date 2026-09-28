/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_shielded_review_app.h"

#include <string.h>

void blue_shielded_review_app_reset(blue_shielded_review_app *app) {
    if (!app) return;
    blue_shielded_review_abort(&app->transaction);
    memset(app->reply, 0, sizeof app->reply);
    memset(app->lines, 0, sizeof app->lines);
    app->detail = app->page = 0;
    strcpy(app->lines[0], "CONNECT Z23");
    strcpy(app->lines[1], "SEND SHIELDED TRANSACTION");
    strcpy(app->lines[2], "READ ONLY; NO SIGNING");
}

static bool show_progress(blue_shielded_review_app *app) {
    zcl_tx_shielded_replay *review = &app->transaction.replay;
    app->page = app->detail = 0;
    return blue_review_screen_progress(review->pass,
        review->wire.received, review->expected, app->lines);
}

static bool show_summary(blue_shielded_review_app *app) {
    app->page = app->detail = 0;
    return blue_review_screen_zip243(app->reply, app->lines);
}

uint16_t blue_shielded_review_app_command(blue_shielded_review_app *app,
    const uint8_t *apdu, size_t apdu_length, uint8_t *reply,
    size_t reply_capacity, size_t *reply_length,
    const zcl_zip243_hasher *hasher) {
    if (!app || !apdu || !reply || !reply_length) {
        blue_shielded_review_app_reset(app);
        if (reply_length) *reply_length = 0;
        return 0x6a80;
    }
    uint8_t instruction = apdu_length >= 2 ? apdu[1] : 0;
    uint16_t status = blue_shielded_review_handle(&app->transaction,
        apdu, apdu_length, reply, reply_capacity, reply_length, hasher);
    if (status != 0x9000) {
        blue_shielded_review_app_reset(app);
        return status;
    }
    bool formatted = true;
    if (instruction == 0x20 || instruction == 0x21 ||
        instruction == 0x22) formatted = show_progress(app);
    else if (instruction == 0x23) {
        formatted = *reply_length == sizeof app->reply;
        if (formatted) {
            memcpy(app->reply, reply, sizeof app->reply);
            formatted = show_summary(app);
        }
    } else if (instruction == 0x24)
        blue_shielded_review_app_reset(app);
    if (formatted) return status;
    blue_shielded_review_app_reset(app);
    *reply_length = 0;
    return 0x6a80;
}

bool blue_shielded_review_app_next(blue_shielded_review_app *app) {
    if (!app) return false;
    if (app->large_text) {
        for (unsigned next = (unsigned)app->detail + 1;
             next < ZCL_BLUE_REVIEW_LINES; ++next)
            if (app->lines[next][0]) {
                app->detail = (uint8_t)next;
                return true;
            }
    }
    if (!app->transaction.complete) {
        app->detail = 0;
        return false;
    }
    app->page ^= 1u;
    app->detail = 0;
    return app->page ?
        blue_review_screen_zip243_digest(app->reply + 44, app->lines) :
        blue_review_screen_zip243(app->reply, app->lines);
}

void blue_shielded_review_app_toggle_text(blue_shielded_review_app *app) {
    if (!app) return;
    app->large_text = !app->large_text;
    app->detail = 0;
}

void blue_shielded_review_app_toggle_dark(blue_shielded_review_app *app) {
    if (app) app->dark = !app->dark;
}
