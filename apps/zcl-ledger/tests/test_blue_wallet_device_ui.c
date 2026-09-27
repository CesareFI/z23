/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "os.h"
#include "blue_bagl_canvas.h"

#undef NDEBUG
#include <assert.h>
#include <stdbool.h>
#include <string.h>

static const bagl_element_t *shown;
static size_t shown_count;
static unsigned displays, exits;
static blue_bagl_canvas *last_canvas;
static const char *fee_snapshot, *totals_snapshot;

void blue_wallet_test_display(const bagl_element_t *elements, size_t count,
    unsigned int (*button)(unsigned int, unsigned int)) {
    assert(button && button(0, 0) == 0);
    blue_bagl_canvas *canvas = blue_bagl_canvas_create(0x1d2028);
    assert(canvas);
    for (size_t i = 0; i < count; ++i) {
        const bagl_element_t *element = &elements[i];
        assert(element->component.x >= 0 && element->component.y >= 0);
        assert(element->component.width > 0 && element->component.height > 0);
        assert(element->component.x + element->component.width <= 320);
        assert(element->component.y + element->component.height <= 480);
        if (!element->text) continue;
        blue_bagl_font font =
            (element->component.font_id & 0xff) ==
                BAGL_FONT_OPEN_SANS_LIGHT_14px ?
            BLUE_BAGL_TEXT_14 : BLUE_BAGL_TEXT_22;
        assert(blue_bagl_text(canvas, element->text,
            element->component.x, element->component.y,
            element->component.width, true,
            element->component.fgcolor, element->component.bgcolor, font));
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
    assert(false);
    return 0;
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
    assert(false);
    return 0;
}

#include "../device-blue-wallet/src/wallet_payment_device.c"

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
    if (fee_snapshot) assert(blue_bagl_write_png(last_canvas, fee_snapshot));
    tap("TOTALS");
    assert(shown == totals_ui);
    assert(find_text("TO OTHER ADDRESSES"));
    assert(find_text("4.00000000 ZCL"));
    assert(find_text("1.00000000 ZCL"));
    assert(find_text("1.23456789 ZCL"));
    if (totals_snapshot)
        assert(blue_bagl_write_png(last_canvas, totals_snapshot));
    tap("BACK");
    assert(shown == fee_ui);
    tap("EXIT");
    assert(exits == 1 && !wallet_payment_visible());
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
    assert(exits == 2);
}

int main(int argc, char **argv) {
    assert(argc == 1 || argc == 3);
    if (argc == 3) {
        fee_snapshot = argv[1];
        totals_snapshot = argv[2];
    }
    test_fee_totals_exit();
    test_invalid_totals_end_review();
    assert(displays == 5);
    blue_bagl_canvas_destroy(last_canvas);
    return 0;
}
