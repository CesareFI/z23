/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "os.h"
#include "blue_bagl_canvas.h"
#include "blue_payment_render.h"
#include "zsha256/zsha256.h"

#undef NDEBUG
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const bagl_element_t *shown;
static size_t shown_count;
static unsigned displays, exits;
unsigned blue_wallet_test_tick_ms;
static blue_bagl_canvas *last_canvas;
static const char *fee_snapshot, *totals_snapshot, *confirmed_snapshot,
    *output_snapshot, *sign_snapshot;

static void expect_pixels(const char *expected) {
    size_t length = 0;
    const uint8_t *rgb = blue_bagl_canvas_rgb(last_canvas, &length);
    assert(rgb && length == 320u * 480u * 3u);
    uint8_t digest[32];
    char hex[65];
    zsha256(rgb, length, digest);
    for (size_t i = 0; i < sizeof digest; ++i)
        assert(snprintf(hex + 2 * i, 3, "%02x", digest[i]) == 2);
    assert(strcmp(hex, expected) == 0);
}

static void expect_preview(bool totals, uint64_t output_zat,
    uint64_t own_output_zat, uint64_t fee_zat, uint8_t paths) {
    blue_bagl_canvas *preview = blue_bagl_canvas_create(0x1d2028);
    assert(preview);
    bool drawn = totals ? blue_payment_render_totals_canvas(preview,
        output_zat, own_output_zat, fee_zat,
        true) : blue_payment_render_fee_canvas(preview,
        fee_zat, paths, true);
    size_t actual_length = 0, preview_length = 0;
    const uint8_t *actual = blue_bagl_canvas_rgb(last_canvas,
        &actual_length);
    const uint8_t *expected = blue_bagl_canvas_rgb(preview,
        &preview_length);
    assert(drawn && actual && expected &&
           actual_length == preview_length &&
           memcmp(actual, expected, actual_length) == 0);
    blue_bagl_canvas_destroy(preview);
}

static void expect_output_preview(const blue_payment_screen *screen) {
    blue_bagl_canvas *preview = blue_bagl_canvas_create(0x1d2028);
    assert(preview);
    assert(blue_payment_render_canvas(preview, screen, true));
    size_t actual_length = 0, preview_length = 0;
    const uint8_t *actual = blue_bagl_canvas_rgb(last_canvas,
        &actual_length);
    const uint8_t *expected = blue_bagl_canvas_rgb(preview,
        &preview_length);
    assert(actual && expected && actual_length == preview_length &&
           memcmp(actual, expected, actual_length) == 0);
    blue_bagl_canvas_destroy(preview);
}

void blue_wallet_test_display(const bagl_element_t *elements, size_t count,
    unsigned int (*button)(unsigned int, unsigned int)) {
    assert(button && button(0, 0) == 0);
    blue_bagl_canvas *canvas = blue_bagl_canvas_create(0x1d2028);
    assert(canvas);
    for (size_t i = 0; i < count; ++i) {
        const bagl_element_t *element = &elements[i];
        int kind = element->component.type & ~BAGL_FLAG_TOUCHABLE;
        assert(element->component.x >= 0 && element->component.y >= 0);
        assert(element->component.width > 0 && element->component.height > 0);
        assert(element->component.x + element->component.width <= 320);
        assert(element->component.y + element->component.height <= 480);
        if (kind == BAGL_RECTANGLE)
            blue_bagl_rectangle(canvas, element->component.x,
                element->component.y, element->component.width,
                element->component.height, element->component.fgcolor);
        if (kind == BAGL_BUTTON)
            blue_bagl_round_rectangle(canvas, element->component.x,
                element->component.y, element->component.width,
                element->component.height, element->component.fgcolor);
        if (!element->text) continue;
        blue_bagl_font font =
            (element->component.font_id & 0xff) ==
                BAGL_FONT_OPEN_SANS_LIGHT_14px ?
            BLUE_BAGL_TEXT_14 : BLUE_BAGL_TEXT_22;
        int text_y = element->component.y;
        if (kind == BAGL_BUTTON &&
            (element->component.font_id & BAGL_FONT_ALIGNMENT_MIDDLE))
            text_y += (element->component.height -
                       blue_bagl_font_height(font)) / 2;
        assert(blue_bagl_text(canvas, element->text,
            element->component.x, text_y, element->component.width, true,
            kind == BAGL_BUTTON ? element->component.bgcolor :
                                  element->component.fgcolor,
            kind == BAGL_BUTTON ? element->component.fgcolor :
                                  element->component.bgcolor, font));
    }
    blue_bagl_canvas_destroy(last_canvas);
    last_canvas = canvas;
    shown = elements;
    shown_count = count;
    ++displays;
}

void os_sched_exit(unsigned code) {
    assert(code == 0);
    ++exits;
}

int cx_blake2b_init2(void *context, unsigned bits, const void *key,
    size_t key_length, const uint8_t *personal, size_t personal_length) {
    (void)context; (void)bits; (void)key; (void)key_length;
    (void)personal; (void)personal_length;
    return CX_BLAKE2B;
}

int cx_hash_sha256(const uint8_t *bytes, unsigned length, uint8_t digest[32]) {
    (void)bytes; (void)length; (void)digest;
    assert(false);
    return 0;
}

int cx_hash(void *context, unsigned mode, const uint8_t *bytes,
    unsigned length, uint8_t *digest, unsigned digest_length) {
    (void)context; (void)mode; (void)bytes; (void)length;
    (void)digest; (void)digest_length;
    assert(false);
    return 0;
}

int cx_sha256_init(void *context) {
    (void)context;
    return CX_SHA256;
}

#include "../device-blue-wallet/src/wallet_payment_device.c"

static unsigned sign_calls;

bool blue_wallet_public_hash160(const uint8_t compressed[33],
    uint8_t hash160[20]) {
    if (compressed[0] != 2 || compressed[1] != 0x33) return false;
    memcpy(hash160, account_hash160, 20);
    return true;
}

bool blue_wallet_sign_digest(void *context, uint8_t path,
    const uint8_t digest[32], uint8_t public_key[33],
    uint8_t signature[BLUE_ECDSA_DER_MAX], size_t *signature_length) {
    (void)context;
    ++sign_calls;
    if (path != BLUE_PAYMENT_INPUT_EXTERNAL || !digest[0]) return false;
    static const uint8_t der[8] = {
        0x30, 0x06, 0x02, 0x01, 0x01, 0x02, 0x01, 0x01
    };
    memset(public_key, 0x33, 33);
    public_key[0] = 2;
    memcpy(signature, der, sizeof der);
    *signature_length = sizeof der;
    return true;
}

static const bagl_element_t *find_text(const char *text) {
    for (size_t i = 0; i < shown_count; ++i)
        if (shown[i].text && strcmp(shown[i].text, text) == 0)
            return &shown[i];
    return NULL;
}

static void tap(const char *text) {
    const bagl_element_t *element = find_text(text);
    assert(element && element->tap);
    (void)element->tap(element);
}

static bool screen_sha256(const uint8_t *bytes, size_t length,
    uint8_t digest[32]) {
    zsha256(bytes, length, digest);
    return true;
}

static void test_boot_workspace(void) {
    memset(&payment, 0xa5, sizeof payment);
    assert((void *)wallet_payment_boot_material() == (void *)&payment);
    wallet_payment_boot_clear();
    const uint8_t *bytes = (const uint8_t *)&payment;
    for (size_t i = 0; i < sizeof *wallet_payment_boot_material(); ++i)
        assert(bytes[i] == 0);
    for (size_t i = sizeof *wallet_payment_boot_material();
         i < sizeof payment; ++i) assert(bytes[i] == 0xa5);
    wallet_payment_abort();
    for (size_t i = 0; i < sizeof payment.input_record; ++i)
        assert(((const uint8_t *)payment.input_record)[i] == 0);
    assert(payment.review.replay.wire.failed);
}

static void test_output_touches(void) {
    uint8_t external[20] = {0}, internal[20] = {0};
    external[0] = 0x11;
    internal[0] = 0x22;
    wallet_payment_set_account_hashes(external, internal);
    wallet_payment_abort();
    payment.active = true;
    payment.review.replay.pass = 3;
    payment.review.total_outputs = 1;
    payment.review.pending = true;
    payment.review.output.amount_zat = 123456789;
    payment.review.output.type = ZCL_TX_STREAM_P2PKH;
    memcpy(payment.review.output.hash160, external, sizeof external);
    assert(blue_payment_screen_format(&payment.review.output, 1,
        screen_sha256, &payment.screen));
    visible = true;
    wallet_payment_display();
    assert(shown == output_ui);
    assert(find_text("OUTPUT 1/1") && find_text("THIS ACCOUNT"));
    assert(find_text("1.23456789 ZCL"));
    assert(find_text(payment.screen.address_lines[0]));
    assert(find_text(payment.screen.address_lines[1]));
    assert(find_text(payment.screen.address_lines[2]));
    expect_output_preview(&payment.screen);
    if (output_snapshot) assert(blue_bagl_write_png(last_canvas,
        output_snapshot));
    const bagl_element_t *stale_continue = find_text("CONTINUE");
    assert(stale_continue);
    tap("CONTINUE");
    assert(shown == waiting_ui && payment.active &&
        !payment.review.pending && payment.review.acknowledged == 1 &&
        payment.own_output_zat == 123456789);
    unsigned after_continue = displays;
    (void)stale_continue->tap(stale_continue);
    assert(shown == waiting_ui && payment.active &&
        payment.review.acknowledged == 1 && displays == after_continue);
    tap("EXIT");
    assert(exits == 1 && !payment.active && !wallet_payment_visible());
    (void)stale_continue->tap(stale_continue);
    assert(displays == after_continue && exits == 1);
}

static void test_fee_totals_exit(void) {
    wallet_payment_abort();
    payment.fee_ready = true;
    payment.fee_zat = 123456789;
    payment.output_zat = 500000000;
    payment.own_output_zat = 100000000;
    payment.input_paths = BLUE_PAYMENT_INPUT_EXTERNAL;
    visible = true;
    wallet_payment_display();
    assert(shown == fee_ui);
    assert(find_text("CALCULATED FEE"));
    assert(find_text("1.23456789 ZCL"));
    assert(find_text("INPUT EXT 0/0"));
    assert(find_text("CHAIN UNCHECKED"));
    assert(find_text("NO SIGNING"));
    expect_pixels("3faaebfca597dfce186112a0fd785173ff772e52b0a3bd83a33bfa96f3fb6449");
    expect_preview(false, payment.output_zat,
        payment.own_output_zat, payment.fee_zat, payment.input_paths);
    if (fee_snapshot) assert(blue_bagl_write_png(last_canvas, fee_snapshot));
    tap("TOTALS");
    assert(shown == totals_ui);
    assert(find_text("TO OTHER ADDRESSES"));
    assert(find_text("4.00000000 ZCL"));
    assert(find_text("1.00000000 ZCL"));
    assert(find_text("1.23456789 ZCL"));
    expect_pixels("9c24ffa29b9745c6519b10c1834dc81a7cc5c4180edee03fc0363af92c23d68d");
    expect_preview(true, payment.output_zat,
        payment.own_output_zat, payment.fee_zat, payment.input_paths);
    if (totals_snapshot)
        assert(blue_bagl_write_png(last_canvas, totals_snapshot));
    tap("BACK");
    assert(shown == fee_ui);
    tap("EXIT");
    assert(exits == 2 && !wallet_payment_visible());
    assert(!payment.fee_ready && !payment.review.verified);
}

static void test_invalid_totals_end_review(void) {
    wallet_payment_abort();
    payment.fee_ready = true;
    payment.fee_zat = 1;
    payment.output_zat = 1;
    payment.own_output_zat = 2;
    payment.input_paths = BLUE_PAYMENT_INPUT_EXTERNAL;
    visible = true;
    wallet_payment_display();
    tap("TOTALS");
    assert(shown == ended_ui);
    assert(find_text("REVIEW ENDED") && find_text("NO SIGNING"));
    assert(!wallet_payment_visible() && !payment.fee_ready);
    tap("EXIT");
    assert(exits == 3);
}

static void test_confirm_review(void) {
    wallet_payment_abort();
    payment.review.verified = true;
    payment.fee_ready = true;
    payment.input_count = 1;
    payment.bound_inputs = 1;
    payment.input_paths = BLUE_PAYMENT_INPUT_EXTERNAL;
    payment.input_record[0][32] = BLUE_PAYMENT_INPUT_EXTERNAL;
    payment.input_zat = 200000000;
    payment.output_zat = 100000000;
    payment.fee_zat = 100000000;
    visible = true;
    wallet_payment_display();
    const bagl_element_t *stale_totals = find_text("TOTALS");
    assert(stale_totals);
    tap("TOTALS");
    assert(shown == totals_ui && find_text("NEXT"));
    const bagl_element_t *stale_back = find_text("BACK");
    const bagl_element_t *stale_next = find_text("NEXT");
    assert(stale_back && stale_next);
    tap("NEXT");
    assert(shown == sign_ui && find_text("NO SIGN") &&
           find_text("SIGN ZCL"));
    const bagl_element_t *stale_no_sign = find_text("NO SIGN");
    tap("NO SIGN");
    assert(shown == confirmed_ui && payment.review_confirmed);
    unsigned completed_displays = displays;
    (void)stale_no_sign->tap(stale_no_sign);
    (void)stale_back->tap(stale_back);
    (void)stale_next->tap(stale_next);
    (void)stale_totals->tap(stale_totals);
    assert(shown == confirmed_ui && payment.review_confirmed &&
           !payment.approved &&
           displays == completed_displays);
    assert(!blue_payment_apdu_touch_approve(&payment));
    uint8_t digest[32] = {0}, path = 0;
    assert(!blue_payment_apdu_take_digest(&payment, 0, digest, &path));
    assert(find_text("REVIEW COMPLETE") && find_text("NO SIGNING"));
    expect_pixels("1973fccae2ddb5e888d3fc1722d8a8007867571e6d73b1a2dbfda70cdb14f923");
    if (confirmed_snapshot)
        assert(blue_bagl_write_png(last_canvas, confirmed_snapshot));
    tap("EXIT");
    assert(exits == 4 && !payment.review_confirmed && !payment.approved);
}

static void test_confirm_requires_totals(void) {
    wallet_payment_abort();
    payment.review.verified = true;
    payment.fee_ready = true;
    payment.input_count = payment.bound_inputs = 1;
    payment.input_paths = BLUE_PAYMENT_INPUT_EXTERNAL;
    visible = true;
    wallet_payment_display();
    (void)confirm_review(NULL);
    assert(shown == ended_ui && !payment.approved);
    tap("EXIT");
    assert(exits == 5);
}

static void expect_cancelled(unsigned previous_exits) {
    tap("EXIT");
    assert(exits == previous_exits + 1 && !wallet_payment_visible());
    assert(!payment.active && !payment.fee_ready &&
           !payment.review.verified && !payment.review_confirmed &&
           !payment.approved);
}

static void test_cancel_staged_pages(void) {
    wallet_payment_abort();
    payment.active = true;
    visible = true;
    wallet_payment_display();
    assert(shown == waiting_ui && find_text("SEND NEXT CHUNK"));
    expect_cancelled(5);

    payment.review.verified = true;
    visible = true;
    wallet_payment_display();
    assert(shown == complete_ui && find_text("CHECKING INPUTS"));
    expect_cancelled(6);

    payment.fee_ready = true;
    payment.fee_zat = 100000000;
    payment.output_zat = 300000000;
    payment.own_output_zat = 100000000;
    payment.input_paths = BLUE_PAYMENT_INPUT_EXTERNAL;
    visible = true;
    wallet_payment_display();
    assert(shown == fee_ui);
    tap("TOTALS");
    assert(shown == totals_ui);
    tap("BACK");
    assert(shown == fee_ui);
    expect_cancelled(7);
}

static void prepare_signing_review(void) {
    wallet_payment_abort();
    payment.review.verified = true;
    payment.fee_ready = true;
    payment.input_count = payment.bound_inputs = 1;
    payment.input_paths = BLUE_PAYMENT_INPUT_EXTERNAL;
    payment.input_record[0][0] = 0x42;
    payment.input_record[0][32] = BLUE_PAYMENT_INPUT_EXTERNAL;
    payment.input_zat = 200000000;
    payment.output_zat = 100000000;
    payment.fee_zat = 100000000;
    visible = true;
    wallet_payment_display();
}

static void check_signing_approval(unsigned prior_calls) {
    assert(shown == signing_ui && payment.approved &&
        sign_calls == prior_calls &&
        find_text("APPROVAL EXPIRES 30S") &&
        blue_wallet_test_tick_ms == 30000);
}

static void check_completed_signature(unsigned prior_calls,
    size_t reply_length) {
    assert(reply_length == 44 && sign_calls == prior_calls + 1 &&
        payment.next_sign_index == 1 && !payment.approved &&
        !blue_wallet_test_tick_ms);
}

static void test_signing_route(void) {
    prepare_signing_review();
    tap("TOTALS");
    tap("NEXT");
    assert(shown == sign_ui && find_text("FINAL PAYMENT CHECK") &&
        find_text("CHAIN UNCHECKED") &&
        find_text("BRANCH UNCHECKED") && !blue_wallet_test_tick_ms);
    expect_pixels("d64862aa9c24cc1839ffd77b17a7eb94dae7dd91aa1553193cdd052135d008bf");
    if (sign_snapshot) assert(blue_bagl_write_png(last_canvas,
        sign_snapshot));
    const bagl_element_t *stale_no_sign = find_text("NO SIGN");
    const bagl_element_t *stale_sign = find_text("SIGN ZCL");
    unsigned before_sign = sign_calls;
    tap("SIGN ZCL");
    check_signing_approval(before_sign);
    (void)stale_no_sign->tap(stale_no_sign);
    (void)stale_sign->tap(stale_sign);
    assert(shown == signing_ui && payment.approved);
    uint8_t frame[BLUE_PAYMENT_SIGN_REPLY_MAX] =
        {0xa5, 0x29, 0, 0, 1, 0};
    size_t reply_length = 0;
    assert(wallet_payment_command(frame, 6, frame, sizeof frame,
        &reply_length) == 0x9000);
    check_completed_signature(before_sign, reply_length);
    wallet_payment_display();
    assert(shown == signed_ui && find_text("SIGNATURES READY"));
    memcpy(frame, (uint8_t[]){0xa5, 0x29, 0, 0, 1, 0}, 6);
    assert(wallet_payment_command(frame, 6, frame, sizeof frame,
        &reply_length) == 0x6985);
    assert(reply_length == 0 && sign_calls == before_sign + 1);
    wallet_payment_display();
    assert(shown == ended_ui && !payment.fee_ready);
    tap("EXIT");
    assert(exits == 9);
}

static void test_signing_requires_touch(void) {
    prepare_signing_review();
    uint8_t frame[BLUE_PAYMENT_SIGN_REPLY_MAX] =
        {0xa5, 0x29, 0, 0, 1, 0};
    size_t reply_length = 99;
    unsigned calls = sign_calls;
    assert(wallet_payment_command(frame, 6, frame, sizeof frame,
        &reply_length) == 0x6985);
    assert(reply_length == 0 && sign_calls == calls &&
        !payment.fee_ready && !payment.approved);
    wallet_payment_display();
    assert(shown == ended_ui);
    tap("EXIT");
    assert(exits == 10);
}

static void test_signing_malformed_frame(void) {
    prepare_signing_review();
    tap("TOTALS");
    tap("NEXT");
    tap("SIGN ZCL");
    uint8_t frame[BLUE_PAYMENT_SIGN_REPLY_MAX] =
        {0xa5, 0x29, 0, 0, 1, 0};
    frame[2] = 1;
    size_t reply_length = 99;
    unsigned calls = sign_calls;
    assert(wallet_payment_command(frame, 6, frame, sizeof frame,
        &reply_length) == 0x6b00);
    assert(reply_length == 0 && sign_calls == calls &&
        !payment.fee_ready && !payment.approved);
    wallet_payment_display();
    assert(shown == ended_ui);
    tap("EXIT");
    assert(exits == 11);
}

static void test_signing_abort_after_approval(void) {
    prepare_signing_review();
    tap("TOTALS");
    tap("NEXT");
    tap("SIGN ZCL");
    assert(payment.approved && shown == signing_ui);
    wallet_payment_abort();
    uint8_t frame[BLUE_PAYMENT_SIGN_REPLY_MAX] =
        {0xa5, 0x29, 0, 0, 1, 0};
    size_t reply_length = 99;
    unsigned calls = sign_calls;
    assert(wallet_payment_command(frame, 6, frame, sizeof frame,
        &reply_length) == 0x6985);
    assert(reply_length == 0 && sign_calls == calls &&
        !payment.approved && !payment.fee_ready);
    wallet_payment_display();
    assert(shown == ended_ui);
    tap("EXIT");
    assert(exits == 12);
}

static void test_new_review_clears_old_signing_view(void) {
    prepare_signing_review();
    tap("TOTALS");
    tap("NEXT");
    tap("SIGN ZCL");
    assert(shown == signing_ui && payment.approved);
    uint8_t begin[17] = {
        0xa5, 0x20, 0, 0, 12,
        136, 0, 0, 0, 0, 0, 0, 0,
        0xbb, 0x09, 0xb8, 0x76
    };
    size_t reply_length = 99;
    unsigned calls = sign_calls;
    assert(wallet_payment_command(begin, sizeof begin, begin,
        sizeof begin, &reply_length) == 0x9000);
    assert(reply_length == 0 && sign_calls == calls);
    wallet_payment_display();
    assert(shown == waiting_ui && payment.active &&
        !payment.approved && !sign_approved_view && !sign_review_view);
    tap("EXIT");
    assert(!wallet_payment_visible());
}

static void test_signing_approval_timer(void) {
    prepare_signing_review();
    tap("TOTALS");
    tap("NEXT");
    tap("SIGN ZCL");
    assert(payment.approved && blue_wallet_test_tick_ms == 30000);
    unsigned calls = sign_calls;
    assert(wallet_payment_timeout());
    assert(!payment.approved && !payment.fee_ready &&
        !wallet_payment_visible() && !blue_wallet_test_tick_ms);
    uint8_t request[BLUE_PAYMENT_SIGN_REPLY_MAX] =
        {0xa5, 0x29, 0, 0, 1, 0};
    size_t reply_length = 99;
    assert(wallet_payment_command(request, 6, request,
        sizeof request, &reply_length) == 0x6985);
    assert(!reply_length && sign_calls == calls);
    assert(!wallet_payment_timeout());
    wallet_payment_abort();
}

static void test_rejected_signing_returns_to_receive(void) {
    wallet_payment_abort();
    payment.active = true;
    payment.approved = true;
    visible = true;
    sign_review_view = true;
    sign_approved_view = true;
    UX_CALLBACK_SET_INTERVAL(1000);
    unsigned before_sign = sign_calls;
    uint8_t malformed[7] = {0xa5, 0x29, 0, 0, 2, 0, 0};
    uint8_t reply[BLUE_PAYMENT_SIGN_REPLY_MAX + 2];
    size_t reply_length = 123;
    assert(wallet_payment_command(malformed, sizeof malformed,
        reply, sizeof reply, &reply_length) == 0x6700);
    assert(reply_length == 0 && !wallet_payment_visible());
    assert(!payment.active && !payment.approved);
    assert(!sign_review_view && !sign_approved_view);
    assert(blue_wallet_test_tick_ms == 0 && sign_calls == before_sign);
}

int main(int argc, char **argv) {
    assert(argc == 1 || argc == 4 || argc == 5 || argc == 6);
    test_boot_workspace();
    if (argc >= 4) {
        fee_snapshot = argv[1];
        totals_snapshot = argv[2];
        confirmed_snapshot = argv[3];
    }
    if (argc == 5) output_snapshot = argv[4];
    if (argc == 6) {
        output_snapshot = argv[4];
        sign_snapshot = argv[5];
    }
    test_output_touches();
    test_fee_totals_exit();
    test_invalid_totals_end_review();
    test_confirm_review();
    test_confirm_requires_totals();
    assert(displays == 13);
    test_cancel_staged_pages();
    assert(displays == 18 && exits == 8);
    test_signing_route();
    test_signing_requires_touch();
    test_signing_malformed_frame();
    test_signing_abort_after_approval();
    test_new_review_clears_old_signing_view();
    test_signing_approval_timer();
    test_rejected_signing_returns_to_receive();
    blue_bagl_canvas_destroy(last_canvas);
    return 0;
}
