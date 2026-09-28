/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLUE_SHIELDED_REVIEW_APP_H
#define ZCL_BLUE_SHIELDED_REVIEW_APP_H

#include "blue_review_screen.h"
#include "blue_shielded_review_apdu.h"

typedef struct {
    blue_shielded_review_state transaction;
    uint8_t reply[76];
    char lines[ZCL_BLUE_REVIEW_LINES][ZCL_BLUE_REVIEW_LINE_SIZE];
    uint8_t detail, page;
    bool large_text, dark;
} blue_shielded_review_app;

void blue_shielded_review_app_reset(blue_shielded_review_app *app);
uint16_t blue_shielded_review_app_command(blue_shielded_review_app *app,
    const uint8_t *apdu, size_t apdu_length, uint8_t *reply,
    size_t reply_capacity, size_t *reply_length,
    const zcl_zip243_hasher *hasher);
bool blue_shielded_review_app_next(blue_shielded_review_app *app);
void blue_shielded_review_app_toggle_text(blue_shielded_review_app *app);
void blue_shielded_review_app_toggle_dark(blue_shielded_review_app *app);

#endif
