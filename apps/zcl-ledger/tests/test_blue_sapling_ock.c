/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_sapling_ock.h"
#include "blue_sapling_aead.h"
#include "zcl_tx_shielded_replay.h"
#include "zcl_zip243_host.h"
#include "crypto/chacha20poly1305.h"
#include "support/log_throttle.h"
#include "base/log_level.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { WIRE_BYTES = 1425, OUTPUT_OFFSET = 412 };

/* Host-only support for the independent core AEAD oracle. */
int64_t clock_now_wall_ms(void) { return 0; }
bool zcl_alloc_fault_should_fail(const char *label) {
    (void)label;
    return false;
}
enum zcl_log_level zcl_log_level_get(void) { return ZCL_LOG_OFF; }
void zcl_log_emit_at(enum zcl_log_level level, const char *format, ...) {
    (void)level;
    (void)format;
}
bool log_throttle_should_emit(struct log_throttle *throttle, uint64_t key,
    int64_t now, int64_t interval, uint64_t *repetitions) {
    (void)throttle; (void)key; (void)now; (void)interval;
    (void)repetitions;
    return false;
}
void memory_cleanse(void *memory, size_t length) {
    volatile uint8_t *bytes = memory;
    for (size_t i = 0; i < length; ++i) bytes[i] = 0;
}

static int hex_digit(int ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}

static void read_fixture(const char *path, uint8_t wire[WIRE_BYTES]) {
    FILE *file = fopen(path, "r");
    assert(file);
    char line[WIRE_BYTES * 2 + 2];
    do {
        assert(fgets(line, sizeof line, file));
    } while (line[0] == '#');
    assert(strlen(line) == WIRE_BYTES * 2 + 1);
    for (size_t i = 0; i < WIRE_BYTES; ++i) {
        int high = hex_digit(line[i * 2]);
        int low = hex_digit(line[i * 2 + 1]);
        assert(high >= 0 && low >= 0);
        wire[i] = (uint8_t)((high << 4) | low);
    }
    assert(line[WIRE_BYTES * 2] == '\n');
    assert(fclose(file) == 0);
}

static bool fail_final(void *context, uint8_t digest[32]) {
    (void)context;
    memset(digest, 0xa5, 32);
    return false;
}

static void capture_verified_output(const uint8_t wire[WIRE_BYTES],
    zcl_tx_shielded_output_capture *output) {
    static const uint8_t expected_digest[32] = {
        0xd4,0x96,0x7a,0x82,0x69,0x00,0x77,0x09,
        0xfd,0x06,0x3a,0x59,0x2f,0x73,0x59,0xb8,
        0x64,0xfa,0x39,0x0c,0x76,0xf4,0x60,0x9d,
        0xc9,0xf9,0xb9,0x60,0x69,0xc2,0x7c,0x8b
    };
    struct blake2b_ctx context;
    zcl_zip243_hasher hasher = zcl_zip243_host_hasher(&context);
    zcl_tx_shielded_replay replay;
    assert(zcl_tx_shielded_replay_begin_output(&replay, WIRE_BYTES,
        0x76b809bbu, &hasher, 0, output));
    for (unsigned pass = 1; pass <= 6; ++pass) {
        for (size_t offset = 0; offset < WIRE_BYTES; offset += 220) {
            size_t take = WIRE_BYTES - offset < 220 ?
                WIRE_BYTES - offset : 220;
            assert(zcl_tx_shielded_replay_feed(&replay,
                wire + offset, take));
        }
        if (pass < 6) assert(zcl_tx_shielded_replay_next(&replay));
    }
    zcl_tx_shielded_facts facts;
    uint8_t digest[32];
    assert(zcl_tx_shielded_replay_finish(&replay, &facts, digest));
    assert(facts.sapling_outputs == 1);
    assert(memcmp(digest, expected_digest, sizeof digest) == 0);
    zcl_tx_shielded_replay_abort(&replay);
}

static void check_note_ciphertext(void) {
    const uint8_t nonce[12] = {0};
    uint8_t key[32], plain[BLUE_SAPLING_NOTE_PLAIN_BYTES];
    uint8_t cipher[BLUE_SAPLING_NOTE_CIPHER_BYTES];
    uint8_t recovered[BLUE_SAPLING_NOTE_PLAIN_BYTES];
    uint8_t core_recovered[BLUE_SAPLING_NOTE_PLAIN_BYTES];
    for (size_t i = 0; i < sizeof key; ++i) key[i] = (uint8_t)i;
    for (size_t i = 0; i < sizeof plain; ++i)
        plain[i] = (uint8_t)(i * 73u + 11u);
    uint8_t zero[BLUE_SAPLING_NOTE_PLAIN_BYTES] = {0};
    uint8_t stream[BLUE_SAPLING_NOTE_PLAIN_BYTES];
    uint8_t zero_cipher[BLUE_SAPLING_NOTE_CIPHER_BYTES];
    assert(chacha20_encrypt(key, 1, nonce, zero, sizeof zero, stream));
    assert(chacha20poly1305_encrypt(stream, sizeof stream, NULL, 0,
        nonce, key, zero_cipher));
    assert(memcmp(zero_cipher, zero, sizeof zero) == 0);
    static const uint8_t tag[16] = {
        0x95,0x22,0xd6,0x67,0x36,0x7b,0x4c,0x44,
        0x02,0xb1,0x2d,0x0b,0x07,0x3e,0x21,0x53
    };
    assert(memcmp(zero_cipher + sizeof zero, tag, sizeof tag) == 0);
    assert(blue_sapling_note_open(recovered, key, zero_cipher));
    assert(memcmp(recovered, stream, sizeof stream) == 0);
    assert(chacha20poly1305_encrypt(plain, sizeof plain, NULL, 0,
        nonce, key, cipher));
    assert(blue_sapling_note_open(recovered, key, cipher));
    assert(chacha20poly1305_decrypt(cipher, sizeof cipher, NULL, 0,
        nonce, key, core_recovered));
    assert(memcmp(recovered, plain, sizeof plain) == 0);
    assert(memcmp(recovered, core_recovered, sizeof recovered) == 0);
    cipher[0] ^= 1u;
    assert(!blue_sapling_note_open(recovered, key, cipher));
    for (size_t i = 0; i < sizeof recovered; ++i) assert(recovered[i] == 0);
    cipher[0] ^= 1u;
    cipher[sizeof cipher - 1] ^= 1u;
    assert(!blue_sapling_note_open(recovered, key, cipher));
    for (size_t i = 0; i < sizeof recovered; ++i) assert(recovered[i] == 0);
    cipher[sizeof cipher - 1] ^= 1u;
    key[0] ^= 1u;
    assert(!blue_sapling_note_open(recovered, key, cipher));
    for (size_t i = 0; i < sizeof recovered; ++i) assert(recovered[i] == 0);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    check_note_ciphertext();
    static const uint8_t ovk[32] = {
        0xce,0x03,0x88,0x0a,0x87,0x50,0x48,0xf6,
        0x17,0x8e,0xbd,0x84,0x2e,0xdb,0xcb,0xd2,
        0x26,0xee,0x7f,0xe5,0x48,0x9d,0x64,0x8a,
        0xec,0x56,0xea,0x8f,0xba,0xf5,0x0f,0x17
    };
    static const uint8_t expected[32] = {
        0xd9,0x53,0x84,0xf8,0x1b,0x92,0xc9,0x07,
        0xc1,0x32,0x79,0x86,0xcb,0x66,0xb7,0x65,
        0x42,0x83,0x73,0x72,0x59,0x5e,0xa0,0x5f,
        0xff,0xec,0x20,0x6d,0x4d,0x44,0x40,0xbb
    };
    uint8_t wire[WIRE_BYTES], key[32];
    read_fixture(argv[1], wire);
    zcl_tx_shielded_output_capture output;
    capture_verified_output(wire, &output);
    assert(memcmp(output.cv, wire + OUTPUT_OFFSET, 32) == 0);
    struct blake2b_ctx context;
    zcl_zip243_hasher hasher = zcl_zip243_host_hasher(&context);
    assert(blue_sapling_ock(key, ovk, output.cv, output.cm,
        output.epk, &hasher));
    assert(memcmp(key, expected, sizeof key) == 0);
    uint8_t outgoing[64];
    const uint8_t *out_ciphertext = output.out_ciphertext;
    assert(blue_sapling_out_open(outgoing, key, out_ciphertext));
    const uint8_t nonce[12] = {0};
    uint8_t core_outgoing[64];
    assert(chacha20poly1305_decrypt(out_ciphertext, 80,
        NULL, 0, nonce, key, core_outgoing));
    assert(memcmp(outgoing, core_outgoing, sizeof outgoing) == 0);
    static const uint8_t expected_outgoing[64] = {
        0x25,0xd4,0xfe,0xd2,0xb7,0xef,0x10,0x5c,
        0xaa,0xe1,0xf6,0x9f,0x11,0x62,0x6d,0x7a,
        0xc7,0x9a,0x51,0xa6,0x9f,0xc2,0x01,0x63,
        0xb4,0x86,0x68,0xd7,0x0f,0xd1,0x0f,0x3a,
        0xca,0xd2,0xf8,0xc7,0x01,0x5b,0xcd,0x97,
        0x59,0x0b,0xf2,0xba,0x2f,0x68,0x30,0x8e,
        0x18,0x6b,0x3d,0x62,0x41,0xa1,0xe8,0x72,
        0x5c,0x21,0x08,0xf3,0x12,0x4b,0xba,0x06
    };
    assert(memcmp(outgoing, expected_outgoing, sizeof outgoing) == 0);
    uint8_t altered_ciphertext[80];
    memcpy(altered_ciphertext, out_ciphertext, sizeof altered_ciphertext);
    altered_ciphertext[79] ^= 1u;
    assert(!blue_sapling_out_open(outgoing, key, altered_ciphertext));
    for (unsigned i = 0; i < sizeof outgoing; ++i)
        assert(outgoing[i] == 0);
    uint8_t wrong_key[32];
    memcpy(wrong_key, key, sizeof wrong_key);
    wrong_key[0] ^= 1u;
    assert(!blue_sapling_out_open(outgoing, wrong_key, out_ciphertext));
    for (unsigned i = 0; i < sizeof outgoing; ++i)
        assert(outgoing[i] == 0);
    memcpy(altered_ciphertext, out_ciphertext, sizeof altered_ciphertext);
    altered_ciphertext[7] ^= 1u;
    assert(!blue_sapling_out_open(outgoing, key, altered_ciphertext));
    for (unsigned i = 0; i < sizeof outgoing; ++i)
        assert(outgoing[i] == 0);
    uint8_t changed_cv[32];
    memcpy(changed_cv, output.cv, sizeof changed_cv);
    changed_cv[0] ^= 1u;
    assert(blue_sapling_ock(key, ovk, changed_cv, output.cm,
        output.epk, &hasher));
    assert(memcmp(key, expected, sizeof key) != 0);
    hasher.final = fail_final;
    assert(!blue_sapling_ock(key, ovk, output.cv, output.cm,
        output.epk, &hasher));
    for (unsigned i = 0; i < sizeof key; ++i) assert(key[i] == 0);
    assert(!blue_sapling_ock(key, ovk, output.cv, output.cm,
        output.epk, NULL));
    for (unsigned i = 0; i < sizeof key; ++i) assert(key[i] == 0);
    memset(&context, 0, sizeof context);
    puts("Blue Sapling outgoing key: passed");
    return 0;
}
