/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zsha256/zsha256.h"
#include "zripemd/zripemd.h"
#include "os.h"
#include "os_io_seproxyhal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define main blue_wallet_device_main
#include "../device-blue-wallet/src/main.c"
#undef main

extern uint8_t _sdata, _edata, _sidata, _sbss, _ebss;
extern uint8_t _stack_bottom, _stack_top;
extern void _exit(int status);
void blue_m3_reset(void);

__attribute__((used, section(".vectors")))
const uintptr_t blue_wallet_m3_vectors[2] = {
    (uintptr_t)&_stack_top, (uintptr_t)blue_m3_reset
};

unsigned char G_io_apdu_buffer[260];
unsigned blue_wallet_test_tick_ms;
static blue_try_context *active_try;
static wallet_boot_material boot_material;
static const bagl_element_t *shown;
static size_t shown_count;
static bool account_ready, exited, failed;
static unsigned request_step, replies_valid, derivations, wipes;

static void uart_text(const char *message) {
    volatile uint32_t *const uart = (volatile uint32_t *)0x40004000u;
    uart[4] = 16u;
    uart[2] = 1u;
    while (*message) {
        while (uart[1] & 1u) {}
        uart[0] = (uint8_t)*message++;
    }
}

static void uart_hex16(unsigned value) {
    static const char digits[] = "0123456789abcdef";
    volatile uint32_t *const uart = (volatile uint32_t *)0x40004000u;
    for (int shift = 12; shift >= 0; shift -= 4) {
        while (uart[1] & 1u) {}
        uart[0] = (uint8_t)digits[(value >> shift) & 15u];
    }
}

void blue_try_enter(blue_try_context *context) {
    context->code = 0;
    context->previous = active_try;
    active_try = context;
}

void blue_try_leave(void) {
    if (!active_try) { failed = true; return; }
    active_try = active_try->previous;
}

blue_try_context *blue_try_current(void) { return active_try; }

void blue_throw(unsigned error) {
    if (!active_try || !error) _exit(1);
    active_try->code = error;
    longjmp(active_try->jump, 1);
}

void blue_wallet_test_display(const bagl_element_t *elements, size_t count,
    unsigned int (*button)(unsigned int, unsigned int)) {
    if (!button || button(0, 0) != 0) failed = true;
    shown = elements;
    shown_count = count;
}

void blue_wallet_test_finger(const unsigned char *buffer) {
    if (!buffer || buffer[0] != SEPROXYHAL_TAG_FINGER_EVENT) failed = true;
    for (size_t i = 0; i < shown_count; ++i) {
        if (shown[i].text && strcmp(shown[i].text, "EXIT") == 0 &&
            shown[i].tap) {
            (void)shown[i].tap(&shown[i]);
            return;
        }
    }
    failed = true;
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
    if (!external || !internal) failed = true;
    account_ready = true;
}

void wallet_payment_abort(void) {}
void wallet_payment_display(void) { failed = true; }
bool wallet_payment_visible(void) { return false; }
bool wallet_payment_timeout(void) { return false; }
uint16_t wallet_payment_command(const uint8_t *apdu, size_t length,
    uint8_t *reply, size_t capacity, size_t *reply_length) {
    (void)apdu; (void)length; (void)reply; (void)capacity;
    *reply_length = 0;
    failed = true;
    return 0x6985;
}
void blue_wallet_signer_wipe(void) { ++wipes; }

int os_global_pin_is_validated(void) { return 1; }

void os_perso_derive_node_bip32(unsigned curve, const unsigned *path,
    unsigned length, uint8_t raw[32], uint8_t chain[32]) {
    if (curve != CX_CURVE_256K1 || length != 5 ||
        path[0] != 0x8000002c || path[1] != 0x80000093 ||
        path[2] != 0x80000000 || path[3] > 1 || path[4] != 0)
        failed = true;
    ++derivations;
    memset(raw, path[3] ? 0x22 : 0x11, 32);
    memset(chain, path[3] ? 0x44 : 0x33, 32);
}

int cx_ecfp_init_private_key(unsigned curve, const uint8_t raw[32],
    unsigned length, cx_ecfp_private_key_t *key) {
    key->curve = curve;
    key->d_len = length;
    memcpy(key->d, raw, 32);
    return 0;
}

int cx_ecfp_generate_pair(unsigned curve, cx_ecfp_public_key_t *public_key,
    cx_ecfp_private_key_t *private_key, int keepprivate) {
    if (curve != CX_CURVE_256K1 || keepprivate != 1) failed = true;
    public_key->curve = curve;
    public_key->W_len = 65;
    public_key->W[0] = 4;
    memcpy(public_key->W + 1, private_key->d, 32);
    memset(public_key->W + 33, private_key->d[0] + 1, 32);
    return 0;
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
    if (((cx_ripemd160_t *)context)->header != CX_RIPEMD160 ||
        mode != CX_LAST) failed = true;
    zripemd160(bytes, length, digest);
    return 20;
}

void os_boot(void) {}
void io_seproxyhal_init(void) {}
void USB_power(int enabled) { if (enabled != 0 && enabled != 1) failed = true; }
void os_sched_exit(unsigned code) { if (code != 0) failed = true; exited = true; }

unsigned short io_exchange(unsigned char channel, unsigned short tx_length) {
    if (tx_length) {
        if (channel != (CHANNEL_APDU | IO_RETURN_AFTER_TX)) failed = true;
        if (request_step == 1 && tx_length == 7 &&
            blue_wallet_identity_matches(G_io_apdu_buffer, tx_length))
            ++replies_valid;
        else if (request_step == 2 && tx_length == 35 &&
            memcmp(G_io_apdu_buffer, wallet_state.public_key, 33) == 0 &&
            G_io_apdu_buffer[33] == 0x90 && G_io_apdu_buffer[34] == 0)
            ++replies_valid;
        else failed = true;
        return 0;
    }
    if (channel != CHANNEL_APDU) failed = true;
    if (request_step++ == 0) {
        const uint8_t identity[] = {0xa5, 0x01, 0, 0, 0};
        memcpy(G_io_apdu_buffer, identity, sizeof identity);
        return sizeof identity;
    }
    if (request_step == 2) {
        const uint8_t address[] = {0xa5, 0x02, 0, 0, 0};
        memcpy(G_io_apdu_buffer, address, sizeof address);
        return sizeof address;
    }
    G_io_seproxyhal_spi_buffer[0] = SEPROXYHAL_TAG_FINGER_EVENT;
    (void)io_event(CHANNEL_SPI);
    blue_throw(0x6812);
}

void io_seproxyhal_spi_send(const unsigned char *bytes,
    unsigned short length) { (void)bytes; (void)length; failed = true; }
unsigned short io_seproxyhal_spi_recv(unsigned char *bytes,
    unsigned short capacity, int flags) {
    (void)bytes; (void)capacity; (void)flags; failed = true; return 0;
}
void io_seproxyhal_display_default(bagl_element_t *element) {
    (void)element; failed = true;
}
int io_seproxyhal_spi_is_status_sent(void) { return 1; }
void io_seproxyhal_general_status(void) { failed = true; }

static bool address_lines_match(void) {
    size_t cursor = 0;
    for (size_t line = 0; line < ZCL_WALLET_ADDRESS_LINES; ++line) {
        size_t i = 0;
        for (; i < ZCL_WALLET_ADDRESS_LINE_SIZE &&
               address_lines[line][i]; ++i) {
            if (cursor >= ZCL_WALLET_ADDRESS_CHARS ||
                address_lines[line][i] != receive_address[cursor++])
                return false;
        }
        if (i == ZCL_WALLET_ADDRESS_LINE_SIZE) return false;
    }
    return cursor == ZCL_WALLET_ADDRESS_CHARS;
}

static void reset_ram(void) {
    uint8_t *source = &_sidata;
    for (volatile uint8_t *target = &_sdata; target < &_edata; ++target)
        *target = *source++;
    for (volatile uint8_t *target = &_sbss; target < &_ebss; ++target)
        *target = 0;
}

static unsigned stack_used(const volatile uint8_t *guard) {
    unsigned lowest = 1536;
    for (unsigned i = 0; i < 1536; ++i)
        if (guard[i] != 0xa5u) { lowest = i; break; }
    return lowest == 1536 ? 512 : 2048u - lowest;
}

static bool boot_material_clear(void) {
    const uint8_t *bytes = (const uint8_t *)&boot_material;
    for (size_t i = 0; i < sizeof boot_material; ++i)
        if (bytes[i]) return false;
    return true;
}

static bool wallet_run_valid(int result, unsigned used) {
    return result == 0 && !failed && !active_try && exited &&
        replies_valid == 2 && account_ready && derivations == 2 &&
        wipes >= 2 && shown == receive_ui && wallet_state.address_ready &&
        strlen(receive_address) == ZCL_WALLET_ADDRESS_CHARS &&
        address_lines_match() && boot_material_clear() && used <= 1536;
}

void blue_m3_reset(void) {
    reset_ram();
    volatile uint8_t *guard = &_stack_bottom;
    volatile uint8_t marker = 0;
    if ((uintptr_t)&marker <= (uintptr_t)guard + 1568u) {
        uart_text("M3 WALLET STACK SETUP FAIL\n");
        _exit(1);
    }
    for (unsigned i = 0; i < 1536; ++i) guard[i] = 0xa5u;
    int result = blue_wallet_device_main();
    unsigned used = stack_used(guard);
    bool valid = wallet_run_valid(result, used);
    uart_text("M3 WALLET STACK 0x");
    uart_hex16(used);
    uart_text(valid ? "\nM3 WALLET PASS\n" : "\nM3 WALLET FAIL\n");
    _exit(valid ? 0 : 1);
}
