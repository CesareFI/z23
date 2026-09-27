/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "os.h"
#include "os_io_seproxyhal.h"
#include "blue_payment_apdu.h"
#include "blue_wallet_layout.h"
#include "wallet_payment_device.h"

#include <string.h>

static blue_payment_apdu payment;
static cx_blake2b_t payment_blake;
static cx_sha256_t payment_sha;
static bool visible;
static uint8_t displayed_view;
static char fee_text[32];
static char others_text[32];
static char own_text[32];
static char input_path_text[20];
static bool totals_view;
static uint8_t account_hash160[20];
static uint8_t internal_hash160[20];
static bool account_ready;

void wallet_payment_set_account_hashes(const uint8_t external_hash160[20],
                                       const uint8_t internal_hash[20]) {
    if (!external_hash160 || !internal_hash) return;
    memcpy(account_hash160, external_hash160, sizeof account_hash160);
    memcpy(internal_hash160, internal_hash, sizeof internal_hash160);
    account_ready = true;
}

static bool hash_sha256(const uint8_t *bytes, size_t length,
    uint8_t digest[32]) {
    return length <= UINT32_MAX &&
        cx_hash_sha256(bytes, (unsigned int)length, digest) == 32;
}

static bool blake_init(void *context, const uint8_t personal[16]) {
    uint8_t copy[16];
    memcpy(copy, personal, sizeof copy);
    return cx_blake2b_init2(context, 256, NULL, 0,
        copy, sizeof copy) == CX_BLAKE2B;
}

static bool hash_update(void *context, const uint8_t *bytes, size_t length) {
    return length <= UINT32_MAX &&
        (cx_hash)(context, 0, bytes, (unsigned int)length, NULL, 0) >= 0;
}

static bool hash_final(void *context, uint8_t digest[32]) {
    return (cx_hash)(context, CX_LAST, NULL, 0, digest, 32) == 32;
}

static bool sha_init(void *context) {
    return cx_sha256_init(context) == CX_SHA256;
}

void wallet_payment_abort(void) {
    blue_payment_apdu_abort(&payment);
    memset(&payment_blake, 0, sizeof payment_blake);
    memset(&payment_sha, 0, sizeof payment_sha);
    memset(fee_text, 0, sizeof fee_text);
    memset(others_text, 0, sizeof others_text);
    memset(own_text, 0, sizeof own_text);
    memset(input_path_text, 0, sizeof input_path_text);
    visible = false;
    displayed_view = 0;
    totals_view = false;
}

bool wallet_payment_visible(void) {
    return visible;
}

uint16_t wallet_payment_command(const uint8_t *apdu, size_t length,
    uint8_t *reply, size_t capacity, size_t *reply_length) {
    if (!account_ready) {
        if (reply_length) *reply_length = 0;
        return 0x6985;
    }
    zcl_zip243_hasher blake = {.context = &payment_blake,
        .init = blake_init, .update = hash_update, .final = hash_final};
    zcl_tx_replay_sha256 sha = {.context = &payment_sha,
        .init = sha_init, .update = hash_update, .final = hash_final};
    blue_payment_owned_hashes owned;
    memcpy(owned.external, account_hash160, sizeof owned.external);
    memcpy(owned.internal, internal_hash160, sizeof owned.internal);
    uint16_t status = blue_payment_apdu_handle(&payment, apdu, length,
        reply, capacity, reply_length, &blake, &sha, hash_sha256, &owned);
    visible = true;
    return status;
}

static const bagl_element_t *exit_review(const bagl_element_t *element) {
    (void)element;
    wallet_payment_abort();
    os_sched_exit(0);
    return NULL;
}

static const bagl_element_t *continue_review(const bagl_element_t *element);
static const bagl_element_t *show_totals(const bagl_element_t *element);
static const bagl_element_t *show_fee(const bagl_element_t *element);
static const bagl_element_t *confirm_review(const bagl_element_t *element);

static unsigned int output_ui_button(unsigned int mask, unsigned int count) {
    (void)mask;
    (void)count;
    return 0;
}

static unsigned int waiting_ui_button(unsigned int mask, unsigned int count) {
    return output_ui_button(mask, count);
}

static unsigned int complete_ui_button(unsigned int mask, unsigned int count) {
    return output_ui_button(mask, count);
}

static unsigned int fee_ui_button(unsigned int mask, unsigned int count) {
    return output_ui_button(mask, count);
}

static unsigned int totals_ui_button(unsigned int mask, unsigned int count) {
    return output_ui_button(mask, count);
}

static unsigned int ended_ui_button(unsigned int mask, unsigned int count) {
    return output_ui_button(mask, count);
}

static unsigned int confirmed_ui_button(unsigned int mask, unsigned int count) {
    return output_ui_button(mask, count);
}

#define BODY ZCL_WALLET_COLOR_BODY
#define TEXT ZCL_WALLET_COLOR_TEXT
#define ACCENT ZCL_WALLET_COLOR_ACCENT
#define BACKGROUND { .component = { .type = BAGL_RECTANGLE, .x = 0, .y = 0, \
    .width = 320, .height = 480, .fill = BAGL_FILL, \
    .fgcolor = BODY, .bgcolor = BODY } }
#define LABEL(top, words, font) { .component = { .type = BAGL_LABEL, \
    .x = 20, .y = (top), .width = 280, .height = 32, \
    .fgcolor = TEXT, .bgcolor = BODY, \
    .font_id = (font) | BAGL_FONT_ALIGNMENT_CENTER }, .text = (words) }
#define BUTTON(left, words, action) { .component = { \
    .type = BAGL_BUTTON | BAGL_FLAG_TOUCHABLE, \
    .x = (left), .y = 386, .width = 135, .height = 58, \
    .radius = 6, .fill = BAGL_FILL, .fgcolor = ACCENT, .bgcolor = BODY, \
    .font_id = BAGL_FONT_OPEN_SANS_LIGHT_16_22PX | \
        BAGL_FONT_ALIGNMENT_CENTER | BAGL_FONT_ALIGNMENT_MIDDLE }, \
    .text = (words), .tap = (action) }

static const bagl_element_t output_ui[] = {
    BACKGROUND,
    LABEL(25, payment.screen.title, BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(68, payment.screen.kind, BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(115, payment.screen.amount, BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(172, "ZCL MAINNET ADDRESS", BAGL_FONT_OPEN_SANS_LIGHT_14px),
    LABEL(208, payment.screen.address_lines[0],
          BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(244, payment.screen.address_lines[1],
          BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(280, payment.screen.address_lines[2],
          BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(331, "DRAFT; NO SIGNING", BAGL_FONT_OPEN_SANS_LIGHT_14px),
    BUTTON(20, "CONTINUE", continue_review),
    BUTTON(165, "EXIT", exit_review)
};

static const bagl_element_t waiting_ui[] = {
    BACKGROUND,
    LABEL(70, "REVIEWING ZCL", BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(150, "SEND NEXT CHUNK", BAGL_FONT_OPEN_SANS_LIGHT_14px),
    LABEL(205, "NO SIGNING", BAGL_FONT_OPEN_SANS_LIGHT_14px),
    BUTTON(165, "EXIT", exit_review)
};

static const bagl_element_t complete_ui[] = {
    BACKGROUND,
    LABEL(90, "CHECKING INPUTS", BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(170, "WAIT FOR Z23", BAGL_FONT_OPEN_SANS_LIGHT_14px),
    LABEL(220, "NO SIGNING", BAGL_FONT_OPEN_SANS_LIGHT_14px),
    BUTTON(165, "EXIT", exit_review)
};

static const bagl_element_t fee_ui[] = {
    BACKGROUND,
    LABEL(70, "CALCULATED FEE", BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(145, fee_text, BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(195, "m/44'/147'/0'", BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(232, input_path_text, BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(283, "CHAIN UNCHECKED", BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(315, "BRANCH UNCHECKED", BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(347, "NO SIGNING", BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    BUTTON(20, "TOTALS", show_totals),
    BUTTON(165, "EXIT", exit_review)
};

static const bagl_element_t totals_ui[] = {
    BACKGROUND,
    LABEL(25, "OUTPUT TOTALS", BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(75, "TO OTHER ADDRESSES", BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(110, others_text, BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(165, "TO YOUR ADDRESSES", BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(200, own_text, BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(250, "FEE", BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(280, fee_text, BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(329, "CHAIN + BRANCH UNCHECKED", BAGL_FONT_OPEN_SANS_LIGHT_14px),
    LABEL(355, "NO SIGNING", BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    BUTTON(20, "BACK", show_fee),
    BUTTON(165, "CONFIRM", confirm_review)
};

static const bagl_element_t confirmed_ui[] = {
    BACKGROUND,
    LABEL(105, "REVIEW CONFIRMED", BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(180, "NO SIGNING", BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(245, "EXIT WHEN DONE", BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    BUTTON(165, "EXIT", exit_review)
};

static const bagl_element_t ended_ui[] = {
    BACKGROUND,
    LABEL(90, "REVIEW ENDED", BAGL_FONT_OPEN_SANS_LIGHT_16_22PX),
    LABEL(170, "NO SIGNING", BAGL_FONT_OPEN_SANS_LIGHT_14px),
    BUTTON(165, "EXIT", exit_review)
};

#undef BUTTON
#undef LABEL
#undef BACKGROUND
#undef ACCENT
#undef TEXT
#undef BODY

static void display_output(void) {
    if (!blue_payment_screen_mark_account(&payment.screen,
            &payment.review.output, account_hash160,
            internal_hash160, account_ready)) {
        wallet_payment_abort();
        UX_DISPLAY(ended_ui, NULL);
        return;
    }
    UX_DISPLAY(output_ui, NULL);
}

static void display_fee(void) {
    const char *paths = blue_payment_input_paths_label(payment.input_paths);
    if (!paths || strlen(paths) >= sizeof input_path_text ||
        !blue_payment_amount_text(payment.fee_zat, fee_text)) {
        wallet_payment_abort();
        UX_DISPLAY(ended_ui, NULL);
        return;
    }
    strcpy(input_path_text, paths);
    UX_DISPLAY(fee_ui, NULL);
}

static void display_totals(void) {
    if (payment.own_output_zat > payment.output_zat ||
        !blue_payment_amount_text(payment.output_zat -
            payment.own_output_zat, others_text) ||
        !blue_payment_amount_text(payment.own_output_zat, own_text) ||
        !blue_payment_amount_text(payment.fee_zat, fee_text)) {
        wallet_payment_abort();
        UX_DISPLAY(ended_ui, NULL);
        return;
    }
    UX_DISPLAY(totals_ui, NULL);
}

void wallet_payment_display(void) {
    uint8_t view = payment.review_confirmed ? 8 :
        payment.review.pending ? 2 :
        payment.fee_ready ? (totals_view ? 7 : 6) :
        payment.review.verified ? 3 :
        payment.active ? 1 : 4;
    if (view == displayed_view) return;
    displayed_view = view;
    if (view == 2) display_output();
    else if (view == 8) { UX_DISPLAY(confirmed_ui, NULL); }
    else if (view == 6) display_fee();
    else if (view == 7) display_totals();
    else if (view == 3) { UX_DISPLAY(complete_ui, NULL); }
    else if (view == 1) { UX_DISPLAY(waiting_ui, NULL); }
    else { UX_DISPLAY(ended_ui, NULL); }
}

static const bagl_element_t *show_totals(const bagl_element_t *element) {
    (void)element;
    if (payment.fee_ready) {
        totals_view = true;
        displayed_view = 0;
        wallet_payment_display();
    }
    return NULL;
}

static const bagl_element_t *show_fee(const bagl_element_t *element) {
    (void)element;
    if (payment.fee_ready) {
        totals_view = false;
        displayed_view = 0;
        wallet_payment_display();
    }
    return NULL;
}

static const bagl_element_t *confirm_review(const bagl_element_t *element) {
    (void)element;
    if (!totals_view || !blue_payment_apdu_touch_confirm(&payment)) {
        wallet_payment_abort();
        UX_DISPLAY(ended_ui, NULL);
        return NULL;
    }
    displayed_view = 0;
    wallet_payment_display();
    return NULL;
}

static const bagl_element_t *continue_review(const bagl_element_t *element) {
    (void)element;
    blue_payment_owned_hashes owned;
    memcpy(owned.external, account_hash160, sizeof owned.external);
    memcpy(owned.internal, internal_hash160, sizeof owned.internal);
    if (!account_ready || !blue_payment_apdu_touch_continue(&payment, &owned))
        blue_payment_apdu_abort(&payment);
    wallet_payment_display();
    return NULL;
}
