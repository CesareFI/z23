/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_address.h"
#include "zsha256/zsha256.h"
#include "zripemd/zripemd.h"

#undef NDEBUG
#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <string.h>

#define main blue_wallet_device_main
#include "../device-blue-wallet/src/main.c"
#undef main

unsigned char G_io_apdu_buffer[260];
unsigned blue_wallet_test_tick_ms;
static jmp_buf request_boundary;
static const bagl_element_t *shown;
static size_t shown_count;
static wallet_boot_material boot_material;
static uint8_t external_hash[20], internal_hash[20];
static unsigned derivations, displays, exits, aborts, signer_wipes;
static bool reject_second_path, reject_pin, reject_init, reject_pair;
static bool hashes_ready, payment_visible, timeout_pending;

void blue_wallet_test_display(const bagl_element_t *elements, size_t count,
    unsigned int (*button)(unsigned int, unsigned int)) {
    assert(button && button(0, 0) == 0);
    shown = elements;
    shown_count = count;
    ++displays;
}

wallet_boot_material *wallet_payment_boot_material(void) {
    return &boot_material;
}

void wallet_payment_boot_clear(void) {
    volatile uint8_t *bytes = (volatile uint8_t *)&boot_material;
    for (size_t i = 0; i < sizeof boot_material; ++i) bytes[i] = 0;
}

void wallet_payment_set_account_hashes(const uint8_t external[20],
    const uint8_t internal[20]) {
    memcpy(external_hash, external, sizeof external_hash);
    memcpy(internal_hash, internal, sizeof internal_hash);
    hashes_ready = true;
}

void wallet_payment_abort(void) { ++aborts; payment_visible = false; }
void wallet_payment_display(void) { assert(false); }
bool wallet_payment_visible(void) { return payment_visible; }
bool wallet_payment_timeout(void) {
    if (!timeout_pending) return false;
    timeout_pending = false;
    wallet_payment_abort();
    return true;
}
uint16_t wallet_payment_command(const uint8_t *apdu, size_t length,
    uint8_t *reply, size_t capacity, size_t *reply_length) {
    (void)apdu; (void)length; (void)reply; (void)capacity;
    (void)reply_length;
    assert(false);
    return 0x6d00;
}
void blue_wallet_signer_wipe(void) { ++signer_wipes; }

int os_global_pin_is_validated(void) {
    return !reject_pin && (!reject_second_path || derivations == 0);
}

void os_perso_derive_node_bip32(unsigned curve, const unsigned *path,
    unsigned length, uint8_t raw[32], uint8_t chain[32]) {
    assert(curve == CX_CURVE_256K1 && length == 5);
    assert(path[0] == 0x8000002c && path[1] == 0x80000093);
    assert(path[2] == 0x80000000 && path[3] <= 1 && path[4] == 0);
    ++derivations;
    memset(raw, path[3] ? 0x22 : 0x11, 32);
    memset(chain, path[3] ? 0x44 : 0x33, 32);
}

int cx_ecfp_init_private_key(unsigned curve, const uint8_t raw[32],
    unsigned length, cx_ecfp_private_key_t *key) {
    assert(curve == CX_CURVE_256K1 && length == 32);
    key->curve = curve;
    key->d_len = length;
    memcpy(key->d, raw, length);
    if (reject_init) key->d_len = 0;
    return 0;
}

int cx_ecfp_generate_pair(unsigned curve, cx_ecfp_public_key_t *public_key,
    cx_ecfp_private_key_t *private_key, int keepprivate) {
    assert(curve == CX_CURVE_256K1 && keepprivate == 1);
    public_key->curve = curve;
    public_key->W_len = 65;
    public_key->W[0] = 4;
    memcpy(public_key->W + 1, private_key->d, 32);
    memset(public_key->W + 33, private_key->d[0] + 1, 32);
    return reject_pair ? -1 : 0;
}

int cx_hash_sha256(const uint8_t *bytes, unsigned length, uint8_t digest[32]) {
    zsha256(bytes, length, digest);
    return 32;
}

int cx_ripemd160_init(cx_ripemd160_t *context) {
    context->header = CX_RIPEMD160;
    return CX_RIPEMD160;
}

int cx_hash5(void *context, unsigned mode, const uint8_t *bytes,
    unsigned length, uint8_t *digest) {
    assert(((cx_ripemd160_t *)context)->header == CX_RIPEMD160);
    assert(mode == CX_LAST);
    zripemd160(bytes, length, digest);
    return 20;
}

void os_boot(void) {}
void io_seproxyhal_init(void) {}
void USB_power(int enabled) { assert(enabled == 0 || enabled == 1); }
void os_sched_exit(unsigned code) { assert(code == 0); ++exits; }
unsigned short io_exchange(unsigned char channel, unsigned short tx_length) {
    assert(channel == CHANNEL_APDU && tx_length == 0);
    longjmp(request_boundary, 1);
}
void io_seproxyhal_spi_send(const unsigned char *bytes,
    unsigned short length) { (void)bytes; (void)length; assert(false); }
unsigned short io_seproxyhal_spi_recv(unsigned char *bytes,
    unsigned short capacity, int flags) {
    (void)bytes; (void)capacity; (void)flags; assert(false); return 0;
}
void io_seproxyhal_display_default(bagl_element_t *element) {
    (void)element; assert(false);
}
int io_seproxyhal_spi_is_status_sent(void) { return 1; }
void io_seproxyhal_general_status(void) { assert(false); }

static const bagl_element_t *find_text(const char *text) {
    for (size_t i = 0; i < shown_count; ++i)
        if (shown[i].text && strcmp(shown[i].text, text) == 0)
            return &shown[i];
    return NULL;
}

void blue_wallet_test_finger(const unsigned char *buffer) {
    assert(buffer[0] == SEPROXYHAL_TAG_FINGER_EVENT);
    const bagl_element_t *exit_button = find_text("EXIT");
    assert(exit_button && exit_button->tap);
    (void)exit_button->tap(exit_button);
}

static void start_until_request(void) {
    shown = NULL;
    shown_count = 0;
    derivations = 0;
    hashes_ready = false;
    memset(external_hash, 0, sizeof external_hash);
    memset(internal_hash, 0, sizeof internal_hash);
    if (setjmp(request_boundary) == 0) {
        (void)blue_wallet_device_main();
        assert(false);
    }
    assert(shown && find_text("EXIT"));
}

static void check_error_startup(unsigned expected_derivations) {
    start_until_request();
    assert(shown == error_ui && !wallet_state.address_ready);
    assert(derivations == expected_derivations && !hashes_ready);
    assert(receive_address[0] == 0 && wallet_state.public_key[0] == 0);
    for (size_t i = 0; i < sizeof address_lines; ++i)
        assert(((const uint8_t *)address_lines)[i] == 0);
    for (size_t i = 0; i < sizeof boot_material; ++i)
        assert(((const uint8_t *)&boot_material)[i] == 0);
    G_io_seproxyhal_spi_buffer[0] = SEPROXYHAL_TAG_FINGER_EVENT;
    (void)io_event(CHANNEL_SPI);
}

int main(void) {
    start_until_request();
    assert(shown == receive_ui && wallet_state.address_ready);
    assert(derivations == 2 && hashes_ready);
    assert(memcmp(external_hash, internal_hash, 20) != 0);
    char expected[ZCL_ADDRESS_SIZE];
    assert(zcl_address_from_hash160(external_hash, false, expected) == 0);
    assert(strcmp(receive_address, expected) == 0);
    char combined[ZCL_WALLET_ADDRESS_CHARS + 1] = {0};
    for (size_t i = 0; i < ZCL_WALLET_ADDRESS_LINES; ++i)
        strcat(combined, address_lines[i]);
    assert(strcmp(combined, expected) == 0);
    for (unsigned event = SEPROXYHAL_TAG_USB_EVENT_RESET;
         event <= SEPROXYHAL_TAG_USB_EVENT_SUSPENDED; ++event) {
        payment_visible = true;
        unsigned before = aborts;
        G_io_seproxyhal_spi_buffer[0] = SEPROXYHAL_TAG_USB_EVENT;
        G_io_seproxyhal_spi_buffer[3] = (uint8_t)event;
        (void)io_event(CHANNEL_SPI);
        assert(!payment_visible && aborts == before + 1);
        assert(shown == receive_ui);
    }
    timeout_pending = true;
    unsigned before = aborts;
    G_io_seproxyhal_spi_buffer[0] = SEPROXYHAL_TAG_TICKER_EVENT;
    (void)io_event(CHANNEL_SPI);
    assert(!timeout_pending && aborts == before + 1);
    assert(shown == receive_ui);
    G_io_seproxyhal_spi_buffer[0] = SEPROXYHAL_TAG_FINGER_EVENT;
    (void)io_event(CHANNEL_SPI);
    assert(exits == 1 && aborts == 4 && signer_wipes == 3);

    reject_second_path = true;
    check_error_startup(1);
    reject_second_path = false;
    reject_init = true;
    check_error_startup(1);
    reject_init = false;
    reject_pair = true;
    check_error_startup(1);
    reject_pair = false;
    reject_pin = true;
    check_error_startup(0);
    assert(displays == 8 && exits == 5);
    assert(aborts == 8 && signer_wipes == 7);
    return 0;
}
